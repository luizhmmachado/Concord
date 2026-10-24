-- Permission hierarchy, member removal, and invite-only joins.
--
-- Closes the gaps left by 0013:
--   * a custom role with manage_roles can no longer grant/assign permissions
--     it does not itself have (no self-escalation);
--   * manage_members can now remove members from a server;
--   * clients can no longer insert an own server_members row directly, so a
--     known server_id is not enough to join without an invite.

-- =========================================================
-- Helpers
-- =========================================================
create or replace function public.is_server_owner(p_server_id uuid, p_user_id uuid)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select exists (
        select 1 from public.servers
        where id = p_server_id and owner_id = p_user_id
    );
$$;

create or replace function public.is_server_owner_or_admin(p_server_id uuid, p_user_id uuid)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select public.is_server_owner(p_server_id, p_user_id)
        or exists (
            select 1 from public.server_members
            where server_id = p_server_id
              and user_id = p_user_id
              and role = 'admin'
        );
$$;

-- True when the caller may hand out every permission in p_permissions.
-- Owners and legacy admins may grant anything; everybody else is limited to
-- the permissions they currently hold through their own roles.
create or replace function public.can_grant_server_permissions(p_server_id uuid,
                                                                p_permissions jsonb)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select public.is_server_owner_or_admin(p_server_id, auth.uid())
        or not exists (
            select 1
            from jsonb_each(coalesce(p_permissions, '{}'::jsonb)) as entry(key, value)
            where entry.value = 'true'::jsonb
              and not public.has_server_permission(p_server_id, entry.key)
        );
$$;

alter function public.is_server_owner(uuid, uuid) owner to postgres;
alter function public.is_server_owner_or_admin(uuid, uuid) owner to postgres;
alter function public.can_grant_server_permissions(uuid, jsonb) owner to postgres;

revoke all on function public.is_server_owner(uuid, uuid) from public;
revoke all on function public.is_server_owner_or_admin(uuid, uuid) from public;
revoke all on function public.can_grant_server_permissions(uuid, jsonb) from public;

grant execute on function public.is_server_owner(uuid, uuid) to authenticated;
grant execute on function public.is_server_owner_or_admin(uuid, uuid) to authenticated;
grant execute on function public.can_grant_server_permissions(uuid, jsonb) to authenticated;

-- =========================================================
-- Role management cannot exceed the manager's own permissions
-- =========================================================
drop policy if exists "managers can create server roles" on public.server_roles;
create policy "managers can create server roles"
    on public.server_roles for insert
    with check (
        public.has_server_permission(server_id, 'manage_roles')
        and public.can_grant_server_permissions(server_id, permissions)
    );

drop policy if exists "managers can update server roles" on public.server_roles;
create policy "managers can update server roles"
    on public.server_roles for update
    using (
        public.has_server_permission(server_id, 'manage_roles')
        and public.can_grant_server_permissions(server_id, permissions)
    )
    with check (
        public.has_server_permission(server_id, 'manage_roles')
        and public.can_grant_server_permissions(server_id, permissions)
    );

drop policy if exists "managers can delete server roles" on public.server_roles;
create policy "managers can delete server roles"
    on public.server_roles for delete
    using (
        public.has_server_permission(server_id, 'manage_roles')
        and public.can_grant_server_permissions(server_id, permissions)
    );

drop policy if exists "managers can assign member roles" on public.server_member_roles;
create policy "managers can assign member roles"
    on public.server_member_roles for insert
    with check (
        public.has_server_permission(server_id, 'manage_roles')
        and exists (
            select 1
            from public.server_roles r
            where r.id = role_id
              and r.server_id = server_member_roles.server_id
              and public.can_grant_server_permissions(
                  server_member_roles.server_id, r.permissions
              )
        )
    );

drop policy if exists "managers can unassign member roles" on public.server_member_roles;
create policy "managers can unassign member roles"
    on public.server_member_roles for delete
    using (
        public.has_server_permission(server_id, 'manage_roles')
        and exists (
            select 1
            from public.server_roles r
            where r.id = role_id
              and r.server_id = server_member_roles.server_id
              and public.can_grant_server_permissions(
                  server_member_roles.server_id, r.permissions
              )
        )
    );

-- =========================================================
-- Member removal (manage_members)
-- =========================================================
drop policy if exists "users can leave a server (delete own membership)" on public.server_members;
drop policy if exists "members can leave and managers can remove members" on public.server_members;
create policy "members can leave and managers can remove members"
    on public.server_members for delete
    using (
        auth.uid() = user_id
        or (
            public.has_server_permission(server_id, 'manage_members')
            and not public.is_server_owner(server_id, user_id)
        )
    );

-- Invite joins are performed by the SECURITY DEFINER join_server_by_invite()
-- function, and owner membership by the on_server_created trigger. Neither
-- needs a client INSERT policy, and keeping one would let anyone who learns a
-- server_id join without an invite.
drop policy if exists "users can join a server (insert own membership)" on public.server_members;
