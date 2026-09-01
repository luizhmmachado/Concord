-- Fixes "infinite recursion detected in policy for relation X" (Postgres
-- error 42P17). The original SELECT policies on server_members,
-- direct_conversation_members and call_participants each queried their OWN
-- table inside an EXISTS() check to test membership - but that subquery is
-- itself subject to the same RLS policy, causing infinite recursion.
--
-- Fix: move the membership check into a SECURITY DEFINER function. Since
-- these tables are owned by the migration role and FORCE ROW LEVEL SECURITY
-- is never enabled, the function body bypasses RLS and breaks the cycle.

create or replace function public.is_member_of_server(p_server_id uuid, p_user_id uuid)
returns boolean as $$
    select exists (
        select 1 from public.server_members
        where server_id = p_server_id and user_id = p_user_id
    );
$$ language sql security definer stable;

create or replace function public.is_conversation_member(p_conversation_id uuid, p_user_id uuid)
returns boolean as $$
    select exists (
        select 1 from public.direct_conversation_members
        where conversation_id = p_conversation_id and user_id = p_user_id
    );
$$ language sql security definer stable;

create or replace function public.is_call_participant(p_call_id uuid, p_user_id uuid)
returns boolean as $$
    select exists (
        select 1 from public.call_participants
        where call_id = p_call_id and user_id = p_user_id
    );
$$ language sql security definer stable;

revoke all on function public.is_member_of_server(uuid, uuid) from public;
revoke all on function public.is_conversation_member(uuid, uuid) from public;
revoke all on function public.is_call_participant(uuid, uuid) from public;
grant execute on function public.is_member_of_server(uuid, uuid) to authenticated;
grant execute on function public.is_conversation_member(uuid, uuid) to authenticated;
grant execute on function public.is_call_participant(uuid, uuid) to authenticated;

drop policy if exists "members readable by server members" on public.server_members;
create policy "members readable by server members"
    on public.server_members for select
    using (public.is_member_of_server(server_id, auth.uid()));

drop policy if exists "conversation members list readable by members" on public.direct_conversation_members;
create policy "conversation members list readable by members"
    on public.direct_conversation_members for select
    using (public.is_conversation_member(conversation_id, auth.uid()));

drop policy if exists "call participants readable by participants" on public.call_participants;
create policy "call participants readable by participants"
    on public.call_participants for select
    using (public.is_call_participant(call_id, auth.uid()));
