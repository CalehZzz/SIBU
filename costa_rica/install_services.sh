#!/usr/bin/env bash
# Instala servicios systemd en la Pi (corren sin PC).
#   bash costa_rica/install_services.sh web     # RECOMENDADO: visión vía Firestore (página SIBU)
#   bash costa_rica/install_services.sh http    # opcional :8080 LAN
#   bash costa_rica/install_services.sh all     # web + bridge + vision-plc (cuando TIA OK)
#   bash costa_rica/install_services.sh stop

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE="${1:-web}"

if [[ ! -f "$ROOT/costa_rica/sibu.env" ]]; then
  echo "Creá costa_rica/sibu.env primero:"
  echo "  cp costa_rica/sibu.env.example costa_rica/sibu.env && nano costa_rica/sibu.env"
  exit 1
fi

if [[ ! -f "$ROOT/serviceAccountKey.json" ]]; then
  echo "Falta serviceAccountKey.json en $ROOT (necesario para vision_firebase / bridge)"
  exit 1
fi

sudo cp "$ROOT/costa_rica/systemd/"*.service /etc/systemd/system/
sudo systemctl daemon-reload

enable_one() {
  local unit="$1"
  sudo systemctl enable --now "$unit"
  sudo systemctl --no-pager --full status "$unit" || true
}

case "$MODE" in
  web)
    echo ">>> Visión vía Firestore (página SIBU 📷, sin :8080)"
    sudo systemctl disable --now sibu-vision-http.service 2>/dev/null || true
    enable_one sibu-vision-firebase.service
    ;;
  http)
    echo ">>> Solo vision-http :8080 (misma LAN)"
    enable_one sibu-vision-http.service
    ;;
  all)
    echo ">>> firebase vision + bridge + vision-plc"
    enable_one sibu-vision-firebase.service
    enable_one sibu-bridge.service
    enable_one sibu-vision-plc.service
    ;;
  stop)
    sudo systemctl disable --now \
      sibu-vision-http.service \
      sibu-vision-firebase.service \
      sibu-bridge.service \
      sibu-vision-plc.service 2>/dev/null || true
    echo "Servicios detenidos"
    ;;
  *)
    echo "Uso: $0 [web|http|all|stop]"
    exit 1
    ;;
esac
