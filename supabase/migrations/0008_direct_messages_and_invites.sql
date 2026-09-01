-- Direct messages (friends) and server invites.
--
-- The tables from 0001 already exist; what is missing is a way to *create* a
-- conversation. 0002 only grants SELECT on direct_conversations and lets a user
-- insert their own row in direct_conversation_members, so a client can never
-- add the other participant. Likewise, "servers readable by members" means a
-- non-member cannot resolve an invite code to a server id.
--
-- Both operations are therefore exposed as SECURITY DEFINER functions that
-- validate the caller themselves, instead of loosening the policies.

-- =========================================================
-- Find (or create) the 1:1 conversation between the caller and a username
-- =========================================================
create or replace function public.start_direct_conversation(p_username text)
returns uuid
language plpgsql
security definer
set search_path = public
as $$
declare
    v_me uuid := auth.uid();
    v_peer uuid;
    v_conversation_id uuid;
begin
    if v_me is null then
        raise exception 'not authenticated';
    end if;

    select id into v_peer
    from public.profiles
    where lower(username) = lower(trim(p_username));

    if v_peer is null then
        raise exception 'usuario nao encontrado';
    end if;

    if v_peer = v_me then
        raise exception 'nao e possivel conversar consigo mesmo';
    end if;

    -- An existing 1:1 conversation is one both users belong to and that has
    -- exactly two members.
    select m1.conversation_id into v_conversation_id
    from public.direct_conversation_members m1
    join public.direct_conversation_members m2
        on m2.conversation_id = m1.conversation_id
    where m1.user_id = v_me
      and m2.user_id = v_peer
      and (
        select count(*) from public.direct_conversation_members m
        where m.conversation_id = m1.conversation_id
      ) = 2
    limit 1;

    if v_conversation_id is not null then
        return v_conversation_id;
    end if;

    insert into public.direct_conversations default values
    returning id into v_conversation_id;

    insert into public.direct_conversation_members (conversation_id, user_id)
    values (v_conversation_id, v_me), (v_conversation_id, v_peer);

    return v_conversation_id;
end;
$$;

alter function public.start_direct_conversation(text) owner to postgres;
revoke all on function public.start_direct_conversation(text) from public;
grant execute on function public.start_direct_conversation(text) to authenticated;

-- =========================================================
-- List the caller's conversations along with the peer and last message
-- =========================================================
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
        lm.content,
        lm.created_at
    from public.direct_conversations c
    join public.direct_conversation_members me
        on me.conversation_id = c.id and me.user_id = auth.uid()
    join public.direct_conversation_members other
        on other.conversation_id = c.id and other.user_id <> auth.uid()
    join public.profiles p on p.id = other.user_id
    left join lateral (
        select dm.content, dm.created_at
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

-- =========================================================
-- Join a server from an invite code
-- =========================================================
create or replace function public.join_server_by_invite(p_invite_code text)
returns uuid
language plpgsql
security definer
set search_path = public
as $$
declare
    v_me uuid := auth.uid();
    v_server_id uuid;
begin
    if v_me is null then
        raise exception 'not authenticated';
    end if;

    select id into v_server_id
    from public.servers
    where invite_code = trim(p_invite_code);

    if v_server_id is null then
        raise exception 'convite invalido';
    end if;

    insert into public.server_members (server_id, user_id, role)
    values (v_server_id, v_me, 'member')
    on conflict (server_id, user_id) do nothing;

    return v_server_id;
end;
$$;

alter function public.join_server_by_invite(text) owner to postgres;
revoke all on function public.join_server_by_invite(text) from public;
grant execute on function public.join_server_by_invite(text) to authenticated;
