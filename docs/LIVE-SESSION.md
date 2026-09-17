# Live session: VNC + noVNC + cloudflared

How the interactive preview environment is wired on the dev VM.

## Stack

```
Chrome/desktop on :0 (Xtigervnc, internal)
        │
x11vnc :0 ── RFB :5900 ── websockify (web root /usr/share/novnc) :6900
        │
cloudflared tunnel --url http://localhost:6900   (quick tunnel)
        │
https://<random>.trycloudflare.com/vnc.html      (public, HTTPS)
```

- **x11vnc** attaches to the existing X display `:0` (1600×1200) and serves
  RFB on `localhost:5900`, auth via `-rfbauth ~/.wolfy-vnc/passwd`.
- **websockify** bridges WebSocket→TCP on `:6900` and serves the noVNC web
  client (`/usr/share/novnc`, Debian `novnc` package).
- **cloudflared quick tunnel** exposes `:6900` on a public
  `*.trycloudflare.com` URL — no Cloudflare account needed. The URL is
  ephemeral; it changes if cloudflared restarts.

## Credentials & security

- VNC password: generated randomly, stored `chmod 600` in
  `~/.wolfy-vnc/passwd` (x11vnc obfuscated format; plaintext alongside in
  `password.txt`, also 600). Never committed to the repo.
- All public traffic is HTTPS through Cloudflare; the VNC password is
  still required inside the noVNC page.
- The tunnel exposes only `localhost:6900` (noVNC/websockify), nothing else.

## Processes

```
x11vnc -display :0 -rfbport 5900 -rfbauth ~/.wolfy-vnc/passwd \
       -forever -shared -xkb -noxdamage -repeat
websockify --web /usr/share/novnc 6900 localhost:5900
cloudflared tunnel --url http://localhost:6900 --no-autoupdate
```

A watchdog (`~/.wolfy-vnc/watchdog.sh`, `nohup`'d) checks every 15s and
restarts any dead component; a cloudflared restart writes the new URL to
`~/.wolfy-vnc/url.txt` and appends to `watchdog.log`.

## Verifying

```sh
curl -s -o /dev/null -w '%{http_code}' \
  https://<url>/vnc.html                        # expect 200
# WebSocket upgrade end-to-end through the tunnel:
curl -s --http1.1 -D - -o /dev/null \
  -H 'Upgrade: websocket' -H 'Connection: Upgrade' \
  -H 'Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==' \
  -H 'Sec-WebSocket-Version: 13' -H 'Sec-WebSocket-Protocol: binary' \
  https://<url>/websockify                      # expect 101
```

Open `https://<url>/vnc.html` in a browser, enter the VNC password, and
the desktop is fully interactive.

## Running Wolfy inside the session

The Wayland path: launch sway nested on the X display
(`WLR_BACKENDS=x11 sway`), build QuickShell + wolfycore against Qt 6.8
(`~/tools/Qt/6.8.3/gcc_64`), then `quickshell -c wolfy` inside sway —
visible in the same noVNC browser window. A named cloudflared tunnel
(requires a Cloudflare account + token) would give a stable URL; the
quick tunnel trades persistence for zero setup.
