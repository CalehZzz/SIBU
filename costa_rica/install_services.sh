#!/usr/bin/env bash
# Instala servicios systemd en la Pi para que corran sin PC.
# Uso:
#   cd ~/SIBU
#   bash costa_rica/install_services.sh          # solo vision-http (recomendado hoy)
#   bash costa_rica/install_services.sh all      # http + bridge + vision-plc (cuando TIA OK)

set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MODE="${1:-http}"

if [[ ! -f "$ROOT/costa_rica/sibu.env" ]]; then
  echo "Creá costa_rica/sibu.env primero:"
  echo "  cp costa_rica/sibu.env.example costa_rica/sibu.env && nano costa_rica/sibu.env"
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
  http)
    echo ">>> Solo vision-http (sin Firebase, sin PLC)"
    enable_one sibu-vision-http.service
    echo "Web: http://$(hostname -I | awk '{print $1}'):8080/"
    ;;
  all)
    echo ">>> http + bridge + vision-plc"
    enable_one sibu-vision-http.service
    enable_one sibu-bridge.service
    enable_one sibu-vision-plc.service
    ;;
  stop)
    sudo systemctl disable --now sibu-vision-http.service sibu-bridge.service sibu-vision-plc.service 2>/dev/null || true
    echo "Servicios detenidos"
    ;;
  *)
    echo "Uso: $0 [http|all|stop]"
    exit 1
    ;;
esac
