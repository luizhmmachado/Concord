-- Lets server members watch voice channel occupancy in realtime.
--
-- 0002 restricted call_participants to the call's own participants. Realtime
-- evaluates RLS per subscriber, so a user who had not joined the call received
-- no postgres_changes at all: the channel list only updated after a manual
-- refresh (which goes through the SECURITY DEFINER listing function instead).
--
-- The check is delegated to a SECURITY DEFINER helper so evaluating the policy
-- does not re-enter the RLS of calls/channels/server_members (see 0006).

create or replace function public.can_see_call(p_call_id uuid)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select exists (
        -- Any member of the server that owns the call's channel.
        select 1
        from public.calls c
        join public.channels ch on ch.id = c.channel_id
        join public.server_members m on m.server_id = ch.server_id
        where c.id = p_call_id and m.user_id = auth.uid()
    ) or exists (
        -- Or any member of the private conversation the call belongs to.
        select 1
        from public.calls c
        join public.direct_conversation_members dcm on dcm.conversation_id = c.conversation_id
        where c.id = p_call_id and dcm.user_id = auth.uid()
    );
$$;

alter function public.can_see_call(uuid) owner to postgres;
revoke all on function public.can_see_call(uuid) from public;
grant execute on function public.can_see_call(uuid) to authenticated;

drop policy if exists "call participants readable by participants" on public.call_participants;

create policy "call participants readable by related users"
    on public.call_participants for select
    using (public.can_see_call(call_id));

-- DELETE events only carry the old row (and can only be filtered by RLS) when
-- the table replicates full rows.
alter table public.call_participants replica identity full;

-- =========================================================
-- Publish mute/camera/screen state so the channel list icons are accurate
-- =========================================================
create or replace function public.set_call_state(
    p_channel_id uuid,
    p_muted boolean,
    p_camera_on boolean,
    p_screen_sharing boolean
)
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

    update public.call_participants
    set is_muted = p_muted,
        is_camera_on = p_camera_on,
        is_screen_sharing = p_screen_sharing
    where call_id = v_call_id and user_id = v_me;
end;
$$;

alter function public.set_call_state(uuid, boolean, boolean, boolean) owner to postgres;
revoke all on function public.set_call_state(uuid, boolean, boolean, boolean) from public;
grant execute on function public.set_call_state(uuid, boolean, boolean, boolean) to authenticated;
