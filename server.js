const express = require('express');
const cors = require('cors');
const fs = require('fs');
const path = require('path');

const app = express();
const PORT = process.env.PORT || 3000;
const DB_FILE = path.join(__dirname, 'devices.json');

app.use(cors());
app.use(express.json());
app.use(express.urlencoded({ extended: true }));
app.use(express.static(path.join(__dirname, 'public')));

// Cargar base de datos persistente en disco
let devices = {};
if (fs.existsSync(DB_FILE)) {
    try {
        devices = JSON.parse(fs.readFileSync(DB_FILE, 'utf8'));
    } catch (e) {
        console.error('Error al leer devices.json:', e);
        devices = {};
    }
}

function saveDB() {
    try {
        fs.writeFileSync(DB_FILE, JSON.stringify(devices, null, 2), 'utf8');
    } catch (e) {
        console.error('Error al guardar devices.json:', e);
    }
}

// =========================================================================
// 1. ENDPOINT PARA RECIBIR PING DE LA PLACA ESP8266 (POST /api/ping)
// =========================================================================
app.post('/api/ping', (req, res) => {
    const deviceId = (req.body.deviceId || req.body.id || '').toString().trim().toUpperCase();
    const boardUptimeMs = parseInt(req.body.uptimeMs || 0, 10);

    if (!deviceId) {
        return res.status(400).json({ error: 'Falta el parámetro deviceId' });
    }

    const now = Date.now();
    const onlineSince = boardUptimeMs > 0 ? (now - boardUptimeMs) : now;

    const existing = devices[deviceId] || {};
    const shouldReset = existing.resetRequested || false;

    devices[deviceId] = {
        deviceId: deviceId,
        lastSeen: now,
        onlineSince: onlineSince,
        resetRequested: false, // Resetear la orden una vez enviada
        ip: req.headers['x-forwarded-for'] || req.socket.remoteAddress,
        updatedAt: new Date(now).toISOString()
    };

    saveDB();

    console.log(`[PING] Dispositivo ${deviceId} activo. ${shouldReset ? '-> ENVIANDO ORDEN DE REINICIO' : ''}`);
    return res.json({ 
        success: true, 
        message: 'Ping recibido correctamente', 
        deviceId, 
        lastSeen: now, 
        onlineSince,
        action: shouldReset ? 'RESET_WIFI' : 'NONE'
    });
});

// =========================================================================
// 2. ENDPOINT PARA REGISTRAR ORDEN DE REINICIO REMOTO (POST /api/reset-wifi)
// =========================================================================
app.post('/api/reset-wifi', (req, res) => {
    const deviceId = (req.body.deviceId || req.body.id || '').toString().trim().toUpperCase();

    if (!deviceId || !devices[deviceId]) {
        return res.status(404).json({ error: 'Dispositivo no encontrado' });
    }

    devices[deviceId].resetRequested = true;
    saveDB();

    console.log(`[ORDEN] Solicitud de reinicio de WiFi registrada para ${deviceId}`);
    return res.json({ success: true, message: 'Orden de reinicio registrada. La placa borrará su WiFi en su próximo reporte.' });
});

// =========================================================================
// 2. ENDPOINT PARA OBTENER TODOS LOS DISPOSITIVOS (GET /api/devices)
// =========================================================================
app.get('/api/devices', (req, res) => {
    const list = Object.values(devices).sort((a, b) => b.lastSeen - a.lastSeen);
    return res.json(list);
});

// =========================================================================
// 3. ENDPOINT PARA CONSULTAR EL ESTADO (GET /api/status/:id)
// =========================================================================
app.get('/api/status/:id', (req, res) => {
    const deviceId = (req.params.id || '').toString().trim().toUpperCase();
    const device = devices[deviceId];

    if (!device) {
        return res.json({
            found: false,
            deviceId: deviceId,
            status: 'unknown',
            message: 'El dispositivo no ha registrado ningún reporte todavía.'
        });
    }

    const now = Date.now();
    const elapsedMs = now - device.lastSeen;
    const isOnline = elapsedMs < 80000; // Menos de 80 segundos (1m 20s)
    const uptimeMs = isOnline ? (now - (device.onlineSince || device.lastSeen)) : 0;

    return res.json({
        found: true,
        deviceId: deviceId,
        lastSeen: device.lastSeen,
        onlineSince: device.onlineSince || device.lastSeen,
        elapsedMs: elapsedMs,
        uptimeMs: uptimeMs,
        status: isOnline ? 'online' : 'offline',
        message: isOnline ? 'HAY LUZ' : 'SE FUE LA LUZ'
    });
});

// Redireccionar rutas limpias
app.get('/', (req, res) => {
    res.sendFile(path.join(__dirname, 'public', 'estado.html'));
});

app.listen(PORT, () => {
    console.log(`====================================================`);
    console.log(` Servidor Monitor de Luz ejecutándose en puerto ${PORT}`);
    console.log(` Endpoint Ping: POST /api/ping`);
    console.log(` Endpoint Estado: GET /api/status/:id`);
    console.log(`====================================================`);
});
