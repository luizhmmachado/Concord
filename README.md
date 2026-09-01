# Concord

Um clone do Discord (chamadas de voz/vídeo, compartilhamento de tela, servidores,
canais, chat e configurações com temas) construído em **Qt 5.15.2 (C++/QML)** com
**Supabase** como backend.

> ⚠️ **Importante — leia antes de compilar**
> - Este código foi escrito neste ambiente **sem acesso a um toolchain Qt/qmake/GStreamer**,
>   portanto **não foi compilado nem testado**. Revise e compile localmente antes de usar.
> - Não consegui acessar o link do Figma fornecido (retornou HTTP 403, provavelmente exige
>   login). A UI implementada usa um design próprio inspirado no Discord (tema escuro,
>   barra de servidores, lista de canais). Ajuste `style/Theme.qml` e as páginas em `ui/`
>   quando tiver acesso ao design real.
> - Não tenho um token/CLI do GitHub configurado neste ambiente, então **não criei o
>   repositório remoto**. Um repositório git local já foi inicializado (`git init`) e o
>   primeiro commit está pronto — veja "Publicar no GitHub" abaixo.

## Arquitetura

```
control/
  database/   SupabaseClient (REST/PostgREST + GoTrue auth), RealtimeClient (Phoenix/WebSocket)
  auth/       AuthController (login/registro/sessão)
  servers/    ServerController + ServerListModel/ChannelListModel (servidores e canais)
  chat/       ChatController + MessageListModel (mensagens em tempo real)
  settings/   SettingsController (tema, cor de destaque, dispositivos)
  calls/      CallController, WebRtcPeerConnection (GStreamer webrtcbin), MediaDeviceController
style/        Theme.qml (singleton de tema/cores)
ui/           Páginas QML (login, registro, sidebar de servidores/canais, chat, chamada, configurações)
supabase/migrations/  SQL para criar o banco de dados
```

### Chamadas de voz/vídeo/tela

Qt 5.15 não tem uma stack WebRTC nativa madura. A solução usa **GStreamer**
(`webrtcbin`) por trás de cada `WebRtcPeerConnection`, uma por participante remoto:

- Áudio do microfone: `autoaudiosrc` + elemento `volume` (mute/unmute instantâneo).
- Câmera: `autovideosrc` + elemento `valve` (liga/desliga sem renegociar SDP).
- Tela: `d3d11screencapturesrc` (Windows), `ximagesrc` (Linux) ou `avfvideosrc` (macOS)
  + `valve` próprio, também sem renegociação.
- Sinalização (SDP/ICE) trafega via **Supabase Realtime Broadcast** em um tópico
  `call:<callId>` (sem servidor de sinalização próprio).

**Limitação conhecida:** a renderização dos vídeos remotos na `CallView.qml` ainda
não está ligada a uma superfície QML (precisaria de uma ponte `appsink` →
`QQuickFramebufferObject`). Hoje a tela de chamada reflete corretamente o estado
(quem está conectado, mutado, com câmera/tela ligada), mas não exibe os pixels do
vídeo remoto. Isso é o próximo passo mais importante a implementar.

### Cuidados com memory leaks (seguidos no código)

- Todo `QNetworkReply` é conectado a `deleteLater()` no seu próprio `finished`.
- `QNetworkAccessManager` é sempre um filho (`parent`) do objeto que o usa.
- Handlers de sinais compartilhados (ex: `SupabaseClient::selectFinished`) são
  conectados **uma única vez no construtor** e despachados por uma tag — nunca
  dentro de métodos que podem ser chamados repetidamente (evita conexões duplicadas).
- `WebRtcPeerConnection` é dono de todos os `GstElement*`/`GstBus*` que obtém e os
  libera (`gst_object_unref`) no destrutor; nunca recebe um `QObject parent` porque
  já é gerenciado por `std::unique_ptr` em `CallController` (evitando double-free).
- `RealtimeClient` fecha o WebSocket no destrutor.

## Configurar o Supabase

1. Crie um projeto em https://supabase.com.
2. No SQL Editor, rode nesta ordem os arquivos de `supabase/migrations/`:
   1. `0001_init.sql` — tabelas (profiles, servers, channels, messages, DMs, calls, settings).
   2. `0002_row_level_security.sql` — políticas de RLS (cada usuário só acessa o que pode).
   3. `0003_realtime.sql` — habilita Realtime nas tabelas usadas pelo chat/chamadas.
3. Em Project Settings → API, copie a **Project URL** e a **anon public key** e cole em
   `main.cpp` (`kSupabaseUrl` / `kSupabaseAnonKey`) — idealmente mova para um arquivo de
   configuração fora do controle de versão antes de publicar.
4. Em Authentication → Providers, habilite "Email" (login por e-mail/senha).

## Compilar

Requisitos: Qt 5.15.2 (qml, quick, quickcontrols2, network, multimedia, websockets),
GStreamer 1.18+ com plugins base/good/bad/ugly (`webrtcbin`, `nice`, `sdp`, `video`).

```powershell
qmake Concord.pro
mingw32-make   # ou nmake / jom, dependendo do kit
```

No Windows, defina a variável de ambiente `GSTREAMER_1_0_ROOT_MINGW_X86_64` apontando
para a instalação do GStreamer (ou edite `GSTREAMER_ROOT` em `Concord.pro`).

## Publicar no GitHub

Este ambiente não tem acesso de escrita à sua conta GitHub. Um repositório git local
já foi inicializado nesta pasta. Para publicar como "Concord":

```powershell
# 1. Crie o repositório vazio "Concord" em https://github.com/new (sem README/gitignore)
# 2. Nesta pasta:
git add -A
git commit -m "Initial Concord scaffold: Supabase backend, servers/channels/chat, settings, WebRTC call architecture"
git branch -M main
git remote add origin https://github.com/<seu-usuario>/Concord.git
git push -u origin main
```

## Próximos passos sugeridos

1. Ligar vídeo remoto/local na UI (appsink → QQuickFramebufferObject).
2. Testar a pipeline GStreamer real (câmera, mic, tela) numa máquina com Qt/GStreamer.
3. Aplicar o design do Figma assim que o acesso estiver disponível.
4. Mover as credenciais do Supabase para fora do código-fonte (arquivo `.env`/config local).
5. Adicionar presença online/offline (Supabase Realtime Presence) e notificações push.
