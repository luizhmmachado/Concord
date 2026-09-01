-- Voice channel presence.
--
-- Until now the participant list only existed in the WebRTC signaling topic, so
-- a user could not see who was in a voice channel without joining it first.
-- These functions persist membership in call_participants (already part of the
-- supabase_realtime publication) so every server member can watch it live.
--
-- They are SECURITY DEFINER because "calls readable by related users" and the
-- call_participants policies are per-participant, which is too narrow for
-- listing a whole server, and because each client would otherwise race to
-- create a duplicate call row for the same channel.

-- =========================================================
-- Join: reuse the channel's open call or create one
-- =========================================================
create or replace function public.join_channel_call(p_channel_id uuid)
returns uuid
language plpgsql
security definer
set search_path = public
as $$
declare
    v_me uuid := auth.uid();
    v_call_id uuid;
begin
    if v_me is null then
        raise exception 'not authenticated';
    end if;

    if not exists (
        select 1
        from public.channels c
        join public.server_members m on m.server_id = c.server_id
        where c.id = p_channel_id and m.user_id = v_me
    ) then
        raise exception 'sem acesso a este canal';
    end if;

    select id into v_call_id
    from public.calls
    where channel_id = p_channel_id and ended_at is null
    order by started_at desc
    limit 1;

    if v_call_id is null then
        insert into public.calls (channel_id, initiator_id)
        values (p_channel_id, v_me)
        returning id into v_call_id;
    end if;

    insert into public.call_participants (call_id, user_id)
    values (v_call_id, v_me)
    on conflict (call_id, user_id) do update set left_at = null;

    return v_call_id;
end;
$$;

alter function public.join_channel_call(uuid) owner to postgres;
revoke all on function public.join_channel_call(uuid) from public;
grant execute on function public.join_channel_call(uuid) to authenticated;

-- =========================================================
-- Leave: drop our row and close the call once it is empty
-- =========================================================
create or replace function public.leave_channel_call(p_channel_id uuid)
returns void
language plpgsql
security definer
set search_path = public
as $$
declare
    v_me uuid := auth.uid();
    v_call_id uuid;
begin
    if v_me is null then
        return;
    end if;

    select id into v_call_id
    from public.calls
    where channel_id = p_channel_id and ended_at is null
    order by started_at desc
    limit 1;

    if v_call_id is null then
        return;
    end if;

    delete from public.call_participants
    where call_id = v_call_id and user_id = v_me;

    if not exists (select 1 from public.call_participants where call_id = v_call_id) then
        update public.calls set ended_at = now() where id = v_call_id;
    end if;
end;
$$;

alter function public.leave_channel_call(uuid) owner to postgres;
revoke all on function public.leave_channel_call(uuid) from public;
grant execute on function public.leave_channel_call(uuid) to authenticated;

-- =========================================================
-- Everyone currently in any voice channel of a server
-- =========================================================
create or replace function public.list_server_call_participants(p_server_id uuid)
returns table (
    channel_id uuid,
    user_id uuid,
    display_name text,
    is_muted boolean,
    is_camera_on boolean,
    is_screen_sharing boolean
)
language sql
stable
security definer
set search_path = public
as $$
    select
        c.channel_id,
        cp.user_id,
        coalesce(p.display_name, p.username),
        cp.is_muted,
        cp.is_camera_on,
        cp.is_screen_sharing
    from public.calls c
    join public.call_participants cp on cp.call_id = c.id
    join public.profiles p on p.id = cp.user_id
    where c.channel_id is not null
      and c.ended_at is null
      and exists (
        select 1 from public.server_members m
        where m.server_id = p_server_id and m.user_id = auth.uid()
      )
      and c.channel_id in (
        select id from public.channels where server_id = p_server_id
      );
$$;

alter function public.list_server_call_participants(uuid) owner to postgres;
revoke all on function public.list_server_call_participants(uuid) from public;
grant execute on function public.list_server_call_participants(uuid) to authenticated;
