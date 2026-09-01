-- Fixes username-based login.
--
-- get_email_for_username() previously ran with the caller's privileges, so an
-- anonymous client (which is what the app is during login) hit the row level
-- security policies on public.profiles, matched zero rows and always returned
-- NULL. The app then reported "Usuário ou senha inválidos" for every valid
-- username.
--
-- Recreating it as SECURITY DEFINER lets the lookup bypass RLS. The function is
-- intentionally narrow: it only resolves an exact username to its e-mail, which
-- is the minimum GoTrue needs to sign the user in. Note this is by design a
-- username -> e-mail oracle for anonymous callers; that is inherent to
-- supporting "log in with your username" and is the same trade-off Discord and
-- similar apps make.

create or replace function public.get_email_for_username(p_username text)
returns text
language sql
stable
security definer
set search_path = public, auth
as $$
    select u.email
    from public.profiles p
    join auth.users u on u.id = p.id
    where lower(p.username) = lower(trim(p_username))
    limit 1;
$$;

-- SECURITY DEFINER only bypasses RLS when the owner is not subject to it.
alter function public.get_email_for_username(text) owner to postgres;

revoke all on function public.get_email_for_username(text) from public;
grant execute on function public.get_email_for_username(text) to anon, authenticated;
