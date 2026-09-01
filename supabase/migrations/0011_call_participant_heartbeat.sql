-- Detects clients that vanished without leaving the call.
--
-- Realtime Presence was tried first, but this project's "client presence rate
-- limit" rejects even the channel join once a client trips it, which made the
-- channel list unusable. A heartbeat column gives the same guarantee without
-- depending on that quota.

alter table public.call_participants
    add column if not exists last_seen timestamptz not null default now();

create or replace function public.heartbeat_call_participant(p_channel_id uuid)
returns void
language plpgsql
security definer
set search_path = public
as $$
declare
    v_me uuid := auth.uid();
begin
    if v_me is null then
        return;
    end if;

    update public.call_participants cp
    set last_seen = now()
    from public.calls c
    where c.id = cp.call_id
      and c.channel_id = p_channel_id
      and c.ended_at is null
      and cp.user_id = v_me;
end;
$$;

alter function public.heartbeat_call_participant(uuid) owner to postgres;
revoke all on function public.heartbeat_call_participant(uuid) from public;
grant execute on function public.heartbeat_call_participant(uuid) to authenticated;

-- Listing now drops participants whose client stopped reporting. The sweep runs
-- here because there is no scheduler configured; every read keeps it tidy.
create or replace function public.list_server_call_participants(p_server_id uuid)
returns table (
    channel_id uuid,
    user_id uuid,
    display_name text,
    is_muted boolean,
    is_camera_on boolean,
    is_screen_sharing boolean
)
language plpgsql
stable
security definer
set search_path = public
as $$
begin
    return query
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
      and cp.last_seen > now() - interval '45 seconds'
      and exists (
        select 1 from public.server_members m
        where m.server_id = p_server_id and m.user_id = auth.uid()
      )
      and c.channel_id in (
        select id from public.channels where server_id = p_server_id
      );
end;
$$;

alter function public.list_server_call_participants(uuid) owner to postgres;
revoke all on function public.list_server_call_participants(uuid) from public;
grant execute on function public.list_server_call_participants(uuid) to authenticated;

-- Deleting the stale rows is what makes other clients get a realtime event.
create or replace function public.sweep_stale_call_participants()
returns void
language sql
security definer
set search_path = public
as $$
    delete from public.call_participants
    where last_seen < now() - interval '45 seconds';
$$;

alter function public.sweep_stale_call_participants() owner to postgres;
revoke all on function public.sweep_stale_call_participants() from public;
grant execute on function public.sweep_stale_call_participants() to authenticated;
