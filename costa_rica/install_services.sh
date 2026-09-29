#!/usr/bin/env bash
# Instala servicios systemd en la Pi (corren sin PC).
#   bash costa_rica/install_services.sh web     # visión Firestore + RFID + bins
#   bash costa_rica/install_services.sh rfid    # solo RFID
#   bash costa_rica/install_services.sh bins    # solo niveles de botes :8082
#   bash costa_rica/install_services.sh http    # opcional :8080 LAN
#   bash costa_rica/install_services.sh all     # web + rfid + bins + bridge + vision-plc
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
  echo "Falta serviceAccountKey.json en $ROOT"
  exit 1
fi

# defaults si faltan en env (systemd vacío rompe ExecStart)
grep -q '^RFID_PORT=' "$ROOT/costa_rica/sibu.env" 2>/dev/null || echo 'RFID_PORT=8081' >> "$ROOT/costa_rica/sibu.env"
grep -q '^RFID_TTL_SEC=' "$ROOT/costa_rica/sibu.env" 2>/dev/null || echo 'RFID_TTL_SEC=300' >> "$ROOT/costa_rica/sibu.env"
grep -q '^BINS_PORT=' "$ROOT/costa_rica/sibu.env" 2>/dev/null || echo 'BINS_PORT=8082' >> "$ROOT/costa_rica/sibu.env"

sudo cp "$ROOT/costa_rica/systemd/"*.service /etc/systemd/system/
sudo systemctl daemon-reload

enable_one() {
  local unit="$1"
  sudo systemctl enable --now "$unit"
  sudo systemctl --no-pager --full status "$unit" || true
}

case "$MODE" in
  web)
    echo ">>> Visión Firestore + RFID gate + bins"
    sudo systemctl disable --now sibu-vision-http.service 2>/dev/null || true
    enable_one sibu-vision-firebase.service
    enable_one sibu-rfid-gate.service
    enable_one sibu-bins.service
    ;;
  rfid)
    echo ">>> Solo RFID gate :8081"
    enable_one sibu-rfid-gate.service
    ;;
  bins)
    echo ">>> Solo bins :8082"
    enable_one sibu-bins.service
    ;;
  http)
    echo ">>> Solo vision-http :8080"
    enable_one sibu-vision-http.service
    ;;
  all)
    echo ">>> vision firebase + rfid + bins + bridge + vision-plc"
    enable_one sibu-vision-firebase.service
    enable_one sibu-rfid-gate.service
    enable_one sibu-bins.service
    enable_one sibu-bridge.service
    enable_one sibu-vision-plc.service
    ;;
  stop)
    sudo systemctl disable --now \
      sibu-vision-http.service \
      sibu-vision-firebase.service \
      sibu-rfid-gate.service \
      sibu-bins.service \
      sibu-bridge.service \
      sibu-vision-plc.service 2>/dev/null || true
    echo "Servicios detenidos"
    ;;
  *)
    echo "Uso: $0 [web|rfid|bins|http|all|stop]"
    exit 1
    ;;
esac
