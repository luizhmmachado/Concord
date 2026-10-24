-- =========================================================
-- Chat images
-- =========================================================

alter table public.messages
    add column if not exists image_url text;

alter table public.direct_messages
    add column if not exists image_url text;

-- Public chat image bucket. Uploads are restricted to a folder named after
-- the authenticated user's id; reads are public so QML Image can load them
-- directly without attaching an Authorization header.
insert into storage.buckets (id, name, public, file_size_limit, allowed_mime_types)
values (
    'chat-images',
    'chat-images',
    true,
    10485760,
    array['image/png', 'image/jpeg', 'image/gif', 'image/webp', 'image/bmp']
)
on conflict (id) do update
set public = excluded.public,
    file_size_limit = excluded.file_size_limit,
    allowed_mime_types = excluded.allowed_mime_types;

drop policy if exists "chat images are publicly readable" on storage.objects;
create policy "chat images are publicly readable"
on storage.objects
for select
using (bucket_id = 'chat-images');

drop policy if exists "users can upload their own chat images" on storage.objects;
create policy "users can upload their own chat images"
on storage.objects
for insert
to authenticated
with check (
    bucket_id = 'chat-images'
    and (storage.foldername(name))[1] = auth.uid()::text
);

-- Image-only direct messages have an empty content column, so surface a
-- readable preview in the conversation list instead of a blank row.
create or replace function public.list_direct_conversations()
returns table (
    conversation_id uuid,
    peer_id uuid,
    peer_username text,
    peer_display_name text,
    last_message text,
    last_message_at timestamptz
)
language sql
stable
security definer
set search_path = public
as $$
    select
        c.id,
        p.id,
        p.username,
        p.display_name,
        coalesce(
            nullif(lm.content, ''),
            case when nullif(lm.image_url, '') is not null then '[Imagem]' else '' end
        ),
        lm.created_at
    from public.direct_conversations c
    join public.direct_conversation_members me
        on me.conversation_id = c.id and me.user_id = auth.uid()
    join public.direct_conversation_members other
        on other.conversation_id = c.id and other.user_id <> auth.uid()
    join public.profiles p on p.id = other.user_id
    left join lateral (
        select dm.content, dm.image_url, dm.created_at
        from public.direct_messages dm
        where dm.conversation_id = c.id
        order by dm.created_at desc
        limit 1
    ) lm on true
    order by coalesce(lm.created_at, c.created_at) desc;
$$;

alter function public.list_direct_conversations() owner to postgres;
revoke all on function public.list_direct_conversations() from public;
grant execute on function public.list_direct_conversations() to authenticated;
