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

    if (!deviceId) {
        return res.status(400).json({ error: 'Falta el parámetro deviceId' });
    }

    const now = Date.now();
    const existing = devices[deviceId];
    
    // Si es la primera vez o si estuvo offline (más de 80 segundos sin ping), reiniciar el contador de "Tiempo con Luz"
    const wasOffline = !existing || (now - existing.lastSeen >= 80000);
    let onlineSince = existing ? (existing.onlineSince || now) : now;
    if (wasOffline) {
        onlineSince = now; // La energía eléctrica acaba de regresar
    }

    devices[deviceId] = {
        deviceId: deviceId,
        lastSeen: now,
        onlineSince: onlineSince,
        ip: req.headers['x-forwarded-for'] || req.socket.remoteAddress,
        updatedAt: new Date(now).toISOString()
    };

    saveDB();

    console.log(`[PING] Dispositivo ${deviceId} activo a las ${new Date(now).toLocaleTimeString()}`);
    return res.json({ success: true, message: 'Ping recibido correctamente', deviceId, lastSeen: now, onlineSince });
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
