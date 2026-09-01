-- Adds an `email` column to profiles (kept in sync from auth.users) plus a
-- SECURITY DEFINER function so unauthenticated clients can resolve a
-- username to an email address to support "sign in with username" without
-- exposing the whole profiles table before login.

alter table public.profiles add column if not exists email text;

-- Backfill for any existing rows (harmless if table is empty).
update public.profiles p
set email = u.email
from auth.users u
where p.id = u.id and p.email is null;

alter table public.profiles alter column email set not null;
create unique index if not exists profiles_email_key on public.profiles (email);

-- Keep handle_new_user() in sync so new signups populate profiles.email too.
create or replace function public.handle_new_user()
returns trigger as $$
begin
    insert into public.profiles (id, username, display_name, email)
    values (
        new.id,
        coalesce(new.raw_user_meta_data ->> 'username', split_part(new.email, '@', 1)),
        coalesce(new.raw_user_meta_data ->> 'display_name', split_part(new.email, '@', 1)),
        new.email
    );
    insert into public.user_settings (user_id) values (new.id);
    return new;
end;
$$ language plpgsql security definer;

-- Resolves a username to its e-mail so the login screen can accept either.
-- SECURITY DEFINER + explicit grant to anon: intentionally callable by
-- unauthenticated clients (needed before a session exists), but only ever
-- returns an email address for a valid, existing username.
create or replace function public.get_email_for_username(p_username text)
returns text as $$
    select email from public.profiles where username = p_username limit 1;
$$ language sql security definer stable;

revoke all on function public.get_email_for_username(text) from public;
grant execute on function public.get_email_for_username(text) to anon, authenticated;
