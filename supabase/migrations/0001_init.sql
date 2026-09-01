-- Concord initial schema
-- Run in Supabase SQL editor or via `supabase db push`

create extension if not exists "pgcrypto";

-- =========================================================
-- Profiles (1:1 with auth.users)
-- =========================================================
create table if not exists public.profiles (
    id uuid primary key references auth.users (id) on delete cascade,
    username text unique not null,
    display_name text not null,
    avatar_url text,
    status text not null default 'offline' check (status in ('online', 'idle', 'dnd', 'offline')),
    created_at timestamptz not null default now()
);

-- Auto-create a profile row when a new auth user signs up
create or replace function public.handle_new_user()
returns trigger as $$
begin
    insert into public.profiles (id, username, display_name)
    values (
        new.id,
        coalesce(new.raw_user_meta_data ->> 'username', split_part(new.email, '@', 1)),
        coalesce(new.raw_user_meta_data ->> 'display_name', split_part(new.email, '@', 1))
    );
    insert into public.user_settings (user_id) values (new.id);
    return new;
end;
$$ language plpgsql security definer;

drop trigger if exists on_auth_user_created on auth.users;
create trigger on_auth_user_created
    after insert on auth.users
    for each row execute procedure public.handle_new_user();

-- =========================================================
-- Servers (guilds) + membership
-- =========================================================
create table if not exists public.servers (
    id uuid primary key default gen_random_uuid(),
    name text not null,
    icon_url text,
    owner_id uuid not null references public.profiles (id) on delete cascade,
    invite_code text unique not null default encode(gen_random_bytes(6), 'hex'),
    created_at timestamptz not null default now()
);

create table if not exists public.server_members (
    server_id uuid not null references public.servers (id) on delete cascade,
    user_id uuid not null references public.profiles (id) on delete cascade,
    role text not null default 'member' check (role in ('owner', 'admin', 'member')),
    joined_at timestamptz not null default now(),
    primary key (server_id, user_id)
);

-- =========================================================
-- Channels (text or voice) within a server
-- =========================================================
create table if not exists public.channels (
    id uuid primary key default gen_random_uuid(),
    server_id uuid not null references public.servers (id) on delete cascade,
    name text not null,
    type text not null default 'text' check (type in ('text', 'voice')),
    position int not null default 0,
    created_at timestamptz not null default now()
);

create table if not exists public.messages (
    id uuid primary key default gen_random_uuid(),
    channel_id uuid not null references public.channels (id) on delete cascade,
    author_id uuid not null references public.profiles (id) on delete cascade,
    content text not null,
    created_at timestamptz not null default now(),
    edited_at timestamptz
);

-- =========================================================
-- Direct (private) conversations and messages
-- =========================================================
create table if not exists public.direct_conversations (
    id uuid primary key default gen_random_uuid(),
    created_at timestamptz not null default now()
);

create table if not exists public.direct_conversation_members (
    conversation_id uuid not null references public.direct_conversations (id) on delete cascade,
    user_id uuid not null references public.profiles (id) on delete cascade,
    primary key (conversation_id, user_id)
);

create table if not exists public.direct_messages (
    id uuid primary key default gen_random_uuid(),
    conversation_id uuid not null references public.direct_conversations (id) on delete cascade,
    author_id uuid not null references public.profiles (id) on delete cascade,
    content text not null,
    created_at timestamptz not null default now()
);

-- =========================================================
-- Calls (voice/video, server-channel based or private) + participants
-- =========================================================
create table if not exists public.calls (
    id uuid primary key default gen_random_uuid(),
    channel_id uuid references public.channels (id) on delete cascade,
    conversation_id uuid references public.direct_conversations (id) on delete cascade,
    initiator_id uuid not null references public.profiles (id),
    started_at timestamptz not null default now(),
    ended_at timestamptz,
    constraint calls_target_check check (
        (channel_id is not null and conversation_id is null) or
        (channel_id is null and conversation_id is not null)
    )
);

create table if not exists public.call_participants (
    call_id uuid not null references public.calls (id) on delete cascade,
    user_id uuid not null references public.profiles (id) on delete cascade,
    joined_at timestamptz not null default now(),
    left_at timestamptz,
    is_muted boolean not null default false,
    is_camera_on boolean not null default false,
    is_screen_sharing boolean not null default false,
    primary key (call_id, user_id)
);

-- =========================================================
-- Per-user settings (theme, devices, notifications)
-- =========================================================
create table if not exists public.user_settings (
    user_id uuid primary key references public.profiles (id) on delete cascade,
    theme text not null default 'dark' check (theme in ('dark', 'light', 'midnight', 'custom')),
    accent_color text not null default '#5865F2',
    notifications_enabled boolean not null default true,
    input_device text,
    output_device text,
    video_device text,
    updated_at timestamptz not null default now()
);

create index if not exists idx_messages_channel on public.messages (channel_id, created_at);
create index if not exists idx_direct_messages_conversation on public.direct_messages (conversation_id, created_at);
create index if not exists idx_server_members_user on public.server_members (user_id);
create index if not exists idx_channels_server on public.channels (server_id, position);
