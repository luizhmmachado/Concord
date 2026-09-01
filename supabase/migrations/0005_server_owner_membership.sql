-- Fixes "create server returns empty row" caused by RLS: PostgREST's
-- `Prefer: return=representation` re-selects the inserted row using the
-- SELECT policy, but the owner wasn't a member yet (that only happened in
-- a second, separate client-side request). A trigger makes membership
-- atomic with server creation, and the SELECT policy also allows the owner
-- to read their own server directly as a safety net.

create or replace function public.handle_new_server()
returns trigger as $$
begin
    insert into public.server_members (server_id, user_id, role)
    values (new.id, new.owner_id, 'owner')
    on conflict (server_id, user_id) do nothing;
    return new;
end;
$$ language plpgsql security definer;

drop trigger if exists on_server_created on public.servers;
create trigger on_server_created
    after insert on public.servers
    for each row execute procedure public.handle_new_server();

drop policy if exists "servers readable by members" on public.servers;
create policy "servers readable by members or owner"
    on public.servers for select
    using (
        owner_id = auth.uid() or exists (
            select 1 from public.server_members m
            where m.server_id = servers.id and m.user_id = auth.uid()
        )
    );
