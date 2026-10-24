-- Voice occupancy moved out of Postgres.
--
-- 0009-0011 implemented it as call_participants rows refreshed by a heartbeat
-- and listed by polling. That cannot be realtime and needs a 45s staleness
-- window. The client now uses Supabase Realtime Presence
-- (realtime:presence:<serverId>), which is what Discord does conceptually:
-- the realtime server owns the ephemeral state, fans out presence_state /
-- presence_diff, and drops a session automatically when its websocket dies.
--
-- Nothing writes call_participants/calls anymore, but the tables are kept so
-- they can back call history later. These presence-only RPCs and the
-- last_seen column are no longer used.

drop function if exists public.sweep_stale_call_participants();
drop function if exists public.list_server_call_participants(uuid);
drop function if exists public.heartbeat_call_participant(uuid);
drop function if exists public.set_call_state(uuid, boolean, boolean, boolean);
drop function if exists public.leave_channel_call(uuid);
drop function if exists public.join_channel_call(uuid);

alter table public.call_participants
    drop column if exists last_seen;
