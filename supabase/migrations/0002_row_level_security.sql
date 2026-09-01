-- Row Level Security policies for Concord

alter table public.profiles enable row level security;
alter table public.servers enable row level security;
alter table public.server_members enable row level security;
alter table public.channels enable row level security;
alter table public.messages enable row level security;
alter table public.direct_conversations enable row level security;
alter table public.direct_conversation_members enable row level security;
alter table public.direct_messages enable row level security;
alter table public.calls enable row level security;
alter table public.call_participants enable row level security;
alter table public.user_settings enable row level security;

-- profiles: anyone authenticated can read; only owner can update their own row
create policy "profiles are readable by authenticated users"
    on public.profiles for select
    using (auth.role() = 'authenticated');

create policy "users can update own profile"
    on public.profiles for update
    using (auth.uid() = id);

-- servers: readable by members, insert by any authenticated user (they become owner)
create policy "servers readable by members"
    on public.servers for select
    using (
        exists (
            select 1 from public.server_members m
            where m.server_id = servers.id and m.user_id = auth.uid()
        )
    );

create policy "authenticated users can create servers"
    on public.servers for insert
    with check (auth.uid() = owner_id);

create policy "owner can update server"
    on public.servers for update
    using (auth.uid() = owner_id);

-- server_members: readable/insertable by members of that server
create policy "members readable by server members"
    on public.server_members for select
    using (
        exists (
            select 1 from public.server_members m
            where m.server_id = server_members.server_id and m.user_id = auth.uid()
        )
    );

create policy "users can join a server (insert own membership)"
    on public.server_members for insert
    with check (auth.uid() = user_id);

create policy "users can leave a server (delete own membership)"
    on public.server_members for delete
    using (auth.uid() = user_id);

-- channels: readable/writable by server members
create policy "channels readable by server members"
    on public.channels for select
    using (
        exists (
            select 1 from public.server_members m
            where m.server_id = channels.server_id and m.user_id = auth.uid()
        )
    );

create policy "admins/owners can create channels"
    on public.channels for insert
    with check (
        exists (
            select 1 from public.server_members m
            where m.server_id = channels.server_id and m.user_id = auth.uid()
              and m.role in ('owner', 'admin')
        )
    );

-- messages: readable/insertable by members of the parent server
create policy "messages readable by server members"
    on public.messages for select
    using (
        exists (
            select 1 from public.channels c
            join public.server_members m on m.server_id = c.server_id
            where c.id = messages.channel_id and m.user_id = auth.uid()
        )
    );

create policy "server members can send messages"
    on public.messages for insert
    with check (
        auth.uid() = author_id and exists (
            select 1 from public.channels c
            join public.server_members m on m.server_id = c.server_id
            where c.id = messages.channel_id and m.user_id = auth.uid()
        )
    );

-- direct conversations: readable/writable only by participants
create policy "conversation members can read conversation"
    on public.direct_conversations for select
    using (
        exists (
            select 1 from public.direct_conversation_members dcm
            where dcm.conversation_id = direct_conversations.id and dcm.user_id = auth.uid()
        )
    );

create policy "conversation members list readable by members"
    on public.direct_conversation_members for select
    using (
        exists (
            select 1 from public.direct_conversation_members dcm
            where dcm.conversation_id = direct_conversation_members.conversation_id
              and dcm.user_id = auth.uid()
        )
    );

create policy "users can add themselves to a conversation"
    on public.direct_conversation_members for insert
    with check (auth.uid() = user_id);

create policy "direct messages readable by conversation members"
    on public.direct_messages for select
    using (
        exists (
            select 1 from public.direct_conversation_members dcm
            where dcm.conversation_id = direct_messages.conversation_id and dcm.user_id = auth.uid()
        )
    );

create policy "conversation members can send direct messages"
    on public.direct_messages for insert
    with check (
        auth.uid() = author_id and exists (
            select 1 from public.direct_conversation_members dcm
            where dcm.conversation_id = direct_messages.conversation_id and dcm.user_id = auth.uid()
        )
    );

-- calls & participants: readable/writable by participants or server members of the target channel
create policy "calls readable by related users"
    on public.calls for select
    using (
        (channel_id is not null and exists (
            select 1 from public.channels c
            join public.server_members m on m.server_id = c.server_id
            where c.id = calls.channel_id and m.user_id = auth.uid()
        )) or
        (conversation_id is not null and exists (
            select 1 from public.direct_conversation_members dcm
            where dcm.conversation_id = calls.conversation_id and dcm.user_id = auth.uid()
        ))
    );

create policy "authenticated users can start calls"
    on public.calls for insert
    with check (auth.uid() = initiator_id);

create policy "call participants readable by participants"
    on public.call_participants for select
    using (
        exists (
            select 1 from public.call_participants cp
            where cp.call_id = call_participants.call_id and cp.user_id = auth.uid()
        )
    );

create policy "users manage their own call participant row"
    on public.call_participants for all
    using (auth.uid() = user_id)
    with check (auth.uid() = user_id);

-- user_settings: only the owner can read/write their settings
create policy "users can read own settings"
    on public.user_settings for select
    using (auth.uid() = user_id);

create policy "users can update own settings"
    on public.user_settings for update
    using (auth.uid() = user_id);

create policy "users can insert own settings"
    on public.user_settings for insert
    with check (auth.uid() = user_id);
