-- Server roles with permissions + member listing for the right sidebar.
--
-- Roles are server-scoped and members can hold several of them, like Discord.
-- Permission checks are delegated to a SECURITY DEFINER helper so the RLS
-- policies never re-enter the RLS of their own tables (see migration 0006).

create table if not exists public.server_roles (
    id uuid primary key default gen_random_uuid(),
    server_id uuid not null references public.servers (id) on delete cascade,
    name text not null,
    color text not null default '#5865F2',
    permissions jsonb not null default '{}'::jsonb,
    position int not null default 0,
    created_at timestamptz not null default now(),
    unique (server_id, name),
    unique (id, server_id)
);

create table if not exists public.server_member_roles (
    server_id uuid not null,
    user_id uuid not null,
    role_id uuid not null,
    assigned_at timestamptz not null default now(),
    primary key (user_id, role_id),
    foreign key (server_id, user_id)
        references public.server_members (server_id, user_id) on delete cascade,
    foreign key (role_id, server_id)
        references public.server_roles (id, server_id) on delete cascade
);

create index if not exists idx_server_roles_server
    on public.server_roles (server_id, position desc);
create index if not exists idx_server_member_roles_server
    on public.server_member_roles (server_id, user_id);
create index if not exists idx_server_member_roles_role
    on public.server_member_roles (role_id);

alter table public.server_roles enable row level security;
alter table public.server_member_roles enable row level security;

-- Realtime needs the whole old row to decide whether a DELETE is visible.
alter table public.server_roles replica identity full;
alter table public.server_member_roles replica identity full;
alter table public.server_members replica identity full;

-- =========================================================
-- Permission helper
-- =========================================================
-- Owners and the legacy "admin" value always have every permission. Custom
-- roles grant a permission when their JSONB object contains "<key>": true.
create or replace function public.has_server_permission(p_server_id uuid, p_permission text)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select exists (
        select 1 from public.servers s
        where s.id = p_server_id and s.owner_id = auth.uid()
    ) or exists (
        select 1 from public.server_members m
        where m.server_id = p_server_id
          and m.user_id = auth.uid()
          and m.role = 'admin'
    ) or exists (
        select 1
        from public.server_member_roles mr
        join public.server_roles r on r.id = mr.role_id
        where mr.server_id = p_server_id
          and mr.user_id = auth.uid()
          and r.permissions @> jsonb_build_object(p_permission, true)
    );
$$;

alter function public.has_server_permission(uuid, text) owner to postgres;
revoke all on function public.has_server_permission(uuid, text) from public;
grant execute on function public.has_server_permission(uuid, text) to authenticated;

-- =========================================================
-- RLS
-- =========================================================
drop policy if exists "server roles readable by members" on public.server_roles;
create policy "server roles readable by members"
    on public.server_roles for select
    using (public.is_member_of_server(server_id, auth.uid()));

drop policy if exists "managers can create server roles" on public.server_roles;
create policy "managers can create server roles"
    on public.server_roles for insert
    with check (public.has_server_permission(server_id, 'manage_roles'));

drop policy if exists "managers can update server roles" on public.server_roles;
create policy "managers can update server roles"
    on public.server_roles for update
    using (public.has_server_permission(server_id, 'manage_roles'))
    with check (public.has_server_permission(server_id, 'manage_roles'));

drop policy if exists "managers can delete server roles" on public.server_roles;
create policy "managers can delete server roles"
    on public.server_roles for delete
    using (public.has_server_permission(server_id, 'manage_roles'));

drop policy if exists "member roles readable by members" on public.server_member_roles;
create policy "member roles readable by members"
    on public.server_member_roles for select
    using (public.is_member_of_server(server_id, auth.uid()));

drop policy if exists "managers can assign member roles" on public.server_member_roles;
create policy "managers can assign member roles"
    on public.server_member_roles for insert
    with check (
        public.has_server_permission(server_id, 'manage_roles')
        and exists (
            select 1 from public.server_roles r
            where r.id = role_id and r.server_id = server_member_roles.server_id
        )
    );

drop policy if exists "managers can unassign member roles" on public.server_member_roles;
create policy "managers can unassign member roles"
    on public.server_member_roles for delete
    using (public.has_server_permission(server_id, 'manage_roles'));

-- Custom roles with manage_channels can now create channels too; owners and
-- legacy admins keep the same access because the helper grants them all.
drop policy if exists "admins/owners can create channels" on public.channels;
drop policy if exists "managers can create channels" on public.channels;
create policy "managers can create channels"
    on public.channels for insert
    with check (public.has_server_permission(server_id, 'manage_channels'));

-- Self-service joins must not choose an elevated legacy role. Owner
-- membership is created by the definer trigger and invite joins go through
-- join_server_by_invite(), both of which bypass this policy.
drop policy if exists "users can join a server (insert own membership)" on public.server_members;
create policy "users can join a server (insert own membership)"
    on public.server_members for insert
    with check (auth.uid() = user_id and role = 'member');

-- =========================================================
-- Member list (one round trip, with aggregated custom roles)
-- =========================================================
create or replace function public.list_server_members(p_server_id uuid)
returns table (
    user_id uuid,
    username text,
    display_name text,
    avatar_url text,
    member_role text,
    roles jsonb
)
language sql
stable
security definer
set search_path = public
as $$
    select
        m.user_id,
        p.username,
        coalesce(p.display_name, p.username),
        p.avatar_url,
        m.role,
        coalesce(
            jsonb_agg(
                jsonb_build_object(
                    'id', r.id,
                    'name', r.name,
                    'color', r.color,
                    'permissions', r.permissions,
                    'position', r.position
                )
                order by r.position desc, lower(r.name)
            ) filter (where r.id is not null),
            '[]'::jsonb
        )
    from public.server_members m
    join public.profiles p on p.id = m.user_id
    left join public.server_member_roles mr
        on mr.server_id = m.server_id and mr.user_id = m.user_id
    left join public.server_roles r on r.id = mr.role_id
    where m.server_id = p_server_id
      and public.is_member_of_server(p_server_id, auth.uid())
    group by m.user_id, p.username, p.display_name, p.avatar_url, m.role
    order by
        case m.role when 'owner' then 0 when 'admin' then 1 else 2 end,
        lower(coalesce(p.display_name, p.username));
$$;

alter function public.list_server_members(uuid) owner to postgres;
revoke all on function public.list_server_members(uuid) from public;
grant execute on function public.list_server_members(uuid) to authenticated;

-- Publish role changes so the member sidebar stays live.
alter publication supabase_realtime add table public.server_roles;
alter publication supabase_realtime add table public.server_member_roles;
