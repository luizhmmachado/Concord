-- Per-server invite permissions.
--
-- Before this migration every member could use the single static
-- servers.invite_code. Now each inviter gets their own token and the server can
-- restrict invitations to specific roles and/or members. The owner/admin can
-- always invite; everyone else follows invite_policy.
--
-- Tokens live in server_invites, which has no client-facing RLS. The only way to
-- read a token is through create_server_invite(), which also enforces the
-- permission check. Keep tokens out of servers so members cannot select them.

-- =========================================================
-- Policy + allowlists
-- =========================================================
alter table public.servers
    add column if not exists invite_policy text not null default 'everyone';

alter table public.servers
    drop constraint if exists servers_invite_policy_check;
alter table public.servers
    add constraint servers_invite_policy_check
    check (invite_policy in ('everyone', 'restricted'));

create table if not exists public.server_invite_roles (
    server_id uuid not null references public.servers (id) on delete cascade,
    role_id uuid not null,
    primary key (server_id, role_id),
    foreign key (role_id, server_id)
        references public.server_roles (id, server_id) on delete cascade
);

create table if not exists public.server_invite_members (
    server_id uuid not null,
    user_id uuid not null,
    primary key (server_id, user_id),
    foreign key (server_id, user_id)
        references public.server_members (server_id, user_id) on delete cascade
);

create table if not exists public.server_invites (
    code text primary key default encode(gen_random_bytes(8), 'hex'),
    server_id uuid not null references public.servers (id) on delete cascade,
    created_by uuid not null references public.profiles (id) on delete cascade,
    created_at timestamptz not null default now(),
    unique (server_id, created_by)
);

create index if not exists idx_server_invites_server
    on public.server_invites (server_id);

-- Migrate the old static code to the owner's token so existing links keep
-- working. Then remove the column so it cannot be read directly by members.
insert into public.server_invites (code, server_id, created_by)
select s.invite_code, s.id, s.owner_id
from public.servers s
where s.invite_code is not null
on conflict (server_id, created_by) do nothing;

-- =========================================================
-- Permission helper
-- =========================================================
create or replace function public.user_can_invite_to_server(p_server_id uuid, p_user_id uuid)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select public.is_member_of_server(p_server_id, p_user_id)
        and (
            public.is_server_owner_or_admin(p_server_id, p_user_id)
            or exists (
                select 1
                from public.servers s
                where s.id = p_server_id
                  and s.invite_policy = 'everyone'
            )
            or exists (
                select 1
                from public.server_invite_members im
                where im.server_id = p_server_id
                  and im.user_id = p_user_id
            )
            or exists (
                select 1
                from public.server_invite_roles ir
                join public.server_member_roles mr
                  on mr.server_id = ir.server_id
                 and mr.role_id = ir.role_id
                where ir.server_id = p_server_id
                  and mr.user_id = p_user_id
            )
        );
$$;

alter function public.user_can_invite_to_server(uuid, uuid) owner to postgres;
revoke all on function public.user_can_invite_to_server(uuid, uuid) from public;

create or replace function public.can_invite_to_server(p_server_id uuid)
returns boolean
language sql
stable
security definer
set search_path = public
as $$
    select public.user_can_invite_to_server(p_server_id, auth.uid());
$$;

alter function public.can_invite_to_server(uuid) owner to postgres;
revoke all on function public.can_invite_to_server(uuid) from public;
grant execute on function public.can_invite_to_server(uuid) to authenticated;

-- =========================================================
-- Join / create invite tokens
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
    v_created_by uuid;
begin
    if v_me is null then
        raise exception 'not authenticated';
    end if;

    select server_id, created_by into v_server_id, v_created_by
    from public.server_invites
    where code = trim(p_invite_code);

    if v_server_id is null
       or not public.user_can_invite_to_server(v_server_id, v_created_by) then
        raise exception 'convite invalido';
    end if;

    insert into public.server_members (server_id, user_id)
    values (v_server_id, v_me)
    on conflict (server_id, user_id) do nothing;

    return v_server_id;
end;
$$;

alter function public.join_server_by_invite(text) owner to postgres;
revoke all on function public.join_server_by_invite(text) from public;
grant execute on function public.join_server_by_invite(text) to authenticated;

create or replace function public.create_server_invite(p_server_id uuid)
returns text
language plpgsql
security definer
set search_path = public
as $$
declare
    v_me uuid := auth.uid();
    v_code text;
begin
    if v_me is null then
        raise exception 'not authenticated';
    end if;

    if not public.can_invite_to_server(p_server_id) then
        raise exception 'sem permissao para convidar';
    end if;

    insert into public.server_invites (server_id, created_by)
    values (p_server_id, v_me)
    on conflict (server_id, created_by)
    do update set server_id = excluded.server_id
    returning code into v_code;

    return v_code;
end;
$$;

alter function public.create_server_invite(uuid) owner to postgres;
revoke all on function public.create_server_invite(uuid) from public;
grant execute on function public.create_server_invite(uuid) to authenticated;

-- =========================================================
-- Read / update invite settings
-- =========================================================
create or replace function public.get_server_invite_settings(p_server_id uuid)
returns jsonb
language sql
stable
security definer
set search_path = public
as $$
    select jsonb_build_object(
        'policy', s.invite_policy,
        'canInvite', public.can_invite_to_server(s.id),
        'canManage', public.has_server_permission(s.id, 'manage_members'),
        'roles', coalesce((
            select jsonb_agg(ir.role_id)
            from public.server_invite_roles ir
            where ir.server_id = s.id
        ), '[]'::jsonb),
        'members', coalesce((
            select jsonb_agg(im.user_id)
            from public.server_invite_members im
            where im.server_id = s.id
        ), '[]'::jsonb)
    )
    from public.servers s
    where s.id = p_server_id
      and public.is_member_of_server(s.id, auth.uid());
$$;

alter function public.get_server_invite_settings(uuid) owner to postgres;
revoke all on function public.get_server_invite_settings(uuid) from public;
grant execute on function public.get_server_invite_settings(uuid) to authenticated;

create or replace function public.update_server_invite_settings(
    p_server_id uuid,
    p_policy text,
    p_role_ids uuid[],
    p_member_ids uuid[]
)
returns jsonb
language plpgsql
security definer
set search_path = public
as $$
begin
    if auth.uid() is null then
        raise exception 'not authenticated';
    end if;

    if not public.has_server_permission(p_server_id, 'manage_members') then
        raise exception 'sem permissao para configurar convites';
    end if;

    if p_policy is null or p_policy not in ('everyone', 'restricted') then
        raise exception 'politica de convite invalida';
    end if;

    update public.servers
    set invite_policy = p_policy
    where id = p_server_id;

    delete from public.server_invite_roles where server_id = p_server_id;
    insert into public.server_invite_roles (server_id, role_id)
    select p_server_id, r.id
    from public.server_roles r
    where r.server_id = p_server_id
      and r.id = any(coalesce(p_role_ids, '{}'::uuid[]));

    delete from public.server_invite_members where server_id = p_server_id;
    insert into public.server_invite_members (server_id, user_id)
    select p_server_id, m.user_id
    from public.server_members m
    where m.server_id = p_server_id
      and m.user_id = any(coalesce(p_member_ids, '{}'::uuid[]));

    return public.get_server_invite_settings(p_server_id);
end;
$$;

alter function public.update_server_invite_settings(uuid, text, uuid[], uuid[]) owner to postgres;
revoke all on function public.update_server_invite_settings(uuid, text, uuid[], uuid[]) from public;
grant execute on function public.update_server_invite_settings(uuid, text, uuid[], uuid[]) to authenticated;

-- =========================================================
-- RLS / Realtime
-- =========================================================
alter table public.server_invite_roles enable row level security;
alter table public.server_invite_members enable row level security;
alter table public.server_invites enable row level security;

alter table public.server_invite_roles replica identity full;
alter table public.server_invite_members replica identity full;

-- Allowlists are server-visible (needed by Realtime), but writes only through
-- the RPC above. Tokens remain private: server_invites has no policies.
drop policy if exists "invite roles readable by members" on public.server_invite_roles;
create policy "invite roles readable by members"
    on public.server_invite_roles for select
    using (public.is_member_of_server(server_id, auth.uid()));

drop policy if exists "invite members readable by members" on public.server_invite_members;
create policy "invite members readable by members"
    on public.server_invite_members for select
    using (public.is_member_of_server(server_id, auth.uid()));

do $$
begin
    alter publication supabase_realtime add table public.servers;
exception when duplicate_object then null;
end $$;

do $$
begin
    alter publication supabase_realtime add table public.server_invite_roles;
exception when duplicate_object then null;
end $$;

do $$
begin
    alter publication supabase_realtime add table public.server_invite_members;
exception when duplicate_object then null;
end $$;

-- The old static code is no longer used by the client.
alter table public.servers drop column if exists invite_code;
