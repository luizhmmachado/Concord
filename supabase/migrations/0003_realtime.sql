-- Enable Supabase Realtime (postgres_changes) on the tables the client subscribes to.
-- Signaling for WebRTC (SDP/ICE) uses Realtime Broadcast on ephemeral topics
-- ("call:<call_id>") and does not require a table.

alter publication supabase_realtime add table public.messages;
alter publication supabase_realtime add table public.direct_messages;
alter publication supabase_realtime add table public.call_participants;
alter publication supabase_realtime add table public.server_members;
