# Concord

Concord e um app desktop em Qt (C++/QML) inspirado no Discord, com autenticacao, servidores, canais, chat em tempo real, chamadas e configuracoes de usuario usando Supabase.

## Downloads

Builds são publicados automaticamente no [GitHub Releases](https://github.com/luizhmmachado/Concord/releases/latest):

- **Windows 10/11 x64**: instalador `.exe` ou arquivo portátil `.zip`.
- **Ubuntu 22.04+ / Debian x64**: pacote `.deb`.

O manifesto estável para landing pages e scripts de atualização fica em
`https://github.com/luizhmmachado/Concord/releases/latest/download/downloads.json`.

## Builds

O workflow `.github/workflows/build.yml` compila e testa as duas plataformas em
cada push e publica os artefatos ao criar uma tag `vX.Y.Z`. Os detalhes de
empacotamento e o contrato de download estão em [`packaging/README.md`](packaging/README.md).
