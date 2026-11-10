const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const OUTPUT_DIR = '/home/alifihsan/Zenith-Dot/assets/logos';
if (!fs.existsSync(OUTPUT_DIR)) {
  fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

// 1. THE OBSIDIAN FAMILIAR (Cyber-Cat / Ferrofluid Cat)
const htmlCat = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 1024px; height: 1024px;
    background: #07090C;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #fff;
    overflow: hidden;
    position: relative;
  }
  .bg-glow {
    position: absolute;
    width: 650px; height: 650px;
    background: radial-gradient(circle, rgba(16, 185, 129, 0.12) 0%, rgba(6, 78, 59, 0.04) 50%, transparent 70%);
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    pointer-events: none;
  }
  .card {
    width: 720px; height: 720px;
    background: linear-gradient(145deg, #12151B 0%, #090B0E 100%);
    border-radius: 160px;
    border: 1px solid rgba(255, 255, 255, 0.1);
    box-shadow: 
      0 30px 80px rgba(0, 0, 0, 0.8),
      0 0 0 1px rgba(16, 185, 129, 0.15),
      inset 0 1px 2px rgba(255, 255, 255, 0.15),
      inset 0 -2px 6px rgba(0, 0, 0, 0.5);
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    position: relative;
    padding: 40px;
  }
  .badge-tag {
    position: absolute;
    top: 52px;
    padding: 6px 16px;
    background: rgba(16, 185, 129, 0.1);
    border: 1px solid rgba(16, 185, 129, 0.25);
    border-radius: 999px;
    font-size: 13px;
    letter-spacing: 2px;
    font-weight: 600;
    color: #34D399;
    text-transform: uppercase;
  }
  .label-group {
    margin-top: 48px;
    text-align: center;
  }
  .title {
    font-size: 34px;
    font-weight: 700;
    letter-spacing: 4px;
    color: #FFFFFF;
    text-transform: uppercase;
  }
  .subtitle {
    margin-top: 8px;
    font-size: 15px;
    font-weight: 500;
    letter-spacing: 3px;
    color: #94A3B8;
    text-transform: uppercase;
  }
</style>
</head>
<body>
  <div class="bg-glow"></div>
  <div class="card">
    <div class="badge-tag">Concept 01 // Companion</div>
    
    <!-- SVG Vector Logo -->
    <svg width="360" height="360" viewBox="0 0 360 360" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <!-- Liquid metallic gradients -->
        <linearGradient id="bodyGrad" x1="60" y1="60" x2="300" y2="300" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="#1E232A" />
          <stop offset="45%" stop-color="#0C0E12" />
          <stop offset="100%" stop-color="#050608" />
        </linearGradient>
        
        <linearGradient id="rimGrad" x1="0" y1="0" x2="360" y2="360" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="rgba(255,255,255,0.4)" />
          <stop offset="30%" stop-color="rgba(52, 211, 153, 0.6)" />
          <stop offset="70%" stop-color="rgba(16, 185, 129, 0.1)" />
          <stop offset="100%" stop-color="rgba(255,255,255,0.05)" />
        </linearGradient>

        <linearGradient id="eyeGrad" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stop-color="#A7F3D0" />
          <stop offset="100%" stop-color="#10B981" />
        </linearGradient>

        <radialGradient id="dotGlow" cx="50%" cy="50%" r="50%">
          <stop offset="0%" stop-color="#34D399" stop-opacity="0.9" />
          <stop offset="60%" stop-color="#10B981" stop-opacity="0.3" />
          <stop offset="100%" stop-color="#059669" stop-opacity="0" />
        </radialGradient>

        <filter id="glowFilter" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="8" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
        
        <filter id="shadowFilter" x="-20%" y="-20%" width="140%" height="140%">
          <feDropShadow dx="0" dy="16" stdDeviation="20" flood-color="#000000" flood-opacity="0.8" />
        </filter>
      </defs>

      <!-- Outer Bezel Dock Notch Silhouette -->
      <path d="M 40 50 Q 180 30 320 50" stroke="rgba(255,255,255,0.08)" stroke-width="2" stroke-linecap="round" />

      <!-- The Cat Island Body: A fluid Pill morphing into Cat Silhouette -->
      <g filter="url(#shadowFilter)">
        <!-- Head + Ears smooth silhouette -->
        <path d="
          M 85 180
          C 85 125, 125 105, 120 70
          C 118 55, 138 60, 155 85
          C 165 95, 195 95, 205 85
          C 222 60, 242 55, 240 70
          C 235 105, 275 125, 275 180
          C 275 235, 235 260, 180 260
          C 125 260, 85 235, 85 180 Z
        " fill="url(#bodyGrad)" stroke="url(#rimGrad)" stroke-width="2.5" />

        <!-- Liquid sheen highlight on top edge -->
        <path d="
          M 125 76 
          C 134 66, 145 74, 153 87
          C 166 100, 194 100, 207 87
          C 215 74, 226 66, 235 76
        " stroke="rgba(255,255,255,0.6)" stroke-width="2" stroke-linecap="round" fill="none" />
      </g>

      <!-- Eyes (Liquid Cyan-Emerald slits, calm and watchful) -->
      <g filter="url(#glowFilter)">
        <!-- Left eye -->
        <path d="M 132 175 Q 150 162 162 175 Q 150 184 132 175 Z" fill="url(#eyeGrad)" />
        <ellipse cx="147" cy="174" rx="2.5" ry="5.5" fill="#042F2E" />

        <!-- Right eye -->
        <path d="M 198 175 Q 210 162 228 175 Q 210 184 198 175 Z" fill="url(#eyeGrad)" />
        <ellipse cx="213" cy="174" rx="2.5" ry="5.5" fill="#042F2E" />
      </g>

      <!-- The Core "Zenith Dot" between / above the eyes -->
      <circle cx="180" cy="132" r="14" fill="url(#dotGlow)" />
      <circle cx="180" cy="132" r="6" fill="#34D399" filter="url(#glowFilter)" />
      <circle cx="180" cy="132" r="2.5" fill="#FFFFFF" />

      <!-- Minimalist Whisker / Soundwave dashes (Audio integration) -->
      <line x1="70" y1="172" x2="98" y2="172" stroke="rgba(52, 211, 153, 0.4)" stroke-width="2" stroke-linecap="round" />
      <line x1="62" y1="184" x2="94" y2="187" stroke="rgba(52, 211, 153, 0.25)" stroke-width="2" stroke-linecap="round" />
      
      <line x1="290" y1="172" x2="262" y2="172" stroke="rgba(52, 211, 153, 0.4)" stroke-width="2" stroke-linecap="round" />
      <line x1="298" y1="184" x2="266" y2="187" stroke="rgba(52, 211, 153, 0.25)" stroke-width="2" stroke-linecap="round" />

      <!-- Subtle nose / chin micro-geometry -->
      <circle cx="180" cy="202" r="2.5" fill="rgba(255,255,255,0.2)" />
    </svg>

    <div class="label-group">
      <div class="title">The Obsidian Familiar</div>
      <div class="subtitle">Ferrofluid Dynamic Companion</div>
    </div>
  </div>
</body>
</html>
`;

// 2. THE CELESTIAL SENTINEL (Orbital Eye / Star Watcher)
const htmlSentinel = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 1024px; height: 1024px;
    background: #05060C;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #fff;
    overflow: hidden;
    position: relative;
  }
  .bg-glow {
    position: absolute;
    width: 700px; height: 700px;
    background: radial-gradient(circle, rgba(99, 102, 241, 0.15) 0%, rgba(56, 189, 248, 0.05) 50%, transparent 70%);
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    pointer-events: none;
  }
  .card {
    width: 720px; height: 720px;
    background: linear-gradient(145deg, #0F1222 0%, #080A14 100%);
    border-radius: 160px;
    border: 1px solid rgba(255, 255, 255, 0.1);
    box-shadow: 
      0 30px 80px rgba(0, 0, 0, 0.85),
      0 0 0 1px rgba(99, 102, 241, 0.2),
      inset 0 1px 2px rgba(255, 255, 255, 0.18),
      inset 0 -2px 6px rgba(0, 0, 0, 0.6);
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    position: relative;
    padding: 40px;
  }
  .badge-tag {
    position: absolute;
    top: 52px;
    padding: 6px 16px;
    background: rgba(99, 102, 241, 0.12);
    border: 1px solid rgba(99, 102, 241, 0.3);
    border-radius: 999px;
    font-size: 13px;
    letter-spacing: 2px;
    font-weight: 600;
    color: #818CF8;
    text-transform: uppercase;
  }
  .label-group {
    margin-top: 48px;
    text-align: center;
  }
  .title {
    font-size: 34px;
    font-weight: 700;
    letter-spacing: 4px;
    color: #FFFFFF;
    text-transform: uppercase;
  }
  .subtitle {
    margin-top: 8px;
    font-size: 15px;
    font-weight: 500;
    letter-spacing: 3px;
    color: #94A3B8;
    text-transform: uppercase;
  }
</style>
</head>
<body>
  <div class="bg-glow"></div>
  <div class="card">
    <div class="badge-tag">Concept 02 // Astronomy</div>
    
    <svg width="360" height="360" viewBox="0 0 360 360" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id="orbitGrad" x1="40" y1="40" x2="320" y2="320" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="#818CF8" />
          <stop offset="50%" stop-color="#38BDF8" />
          <stop offset="100%" stop-color="#4F46E5" />
        </linearGradient>

        <linearGradient id="pillGrad" x1="80" y1="140" x2="280" y2="220" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="#1E1E38" />
          <stop offset="100%" stop-color="#0A0B16" />
        </linearGradient>

        <radialGradient id="sentinelCore" cx="50%" cy="50%" r="50%">
          <stop offset="0%" stop-color="#60A5FA" />
          <stop offset="40%" stop-color="#818CF8" />
          <stop offset="100%" stop-color="#312E81" />
        </radialGradient>

        <filter id="glowBlue" x="-30%" y="-30%" width="160%" height="160%">
          <feGaussianBlur stdDeviation="10" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      <!-- Astrolabe / Celestial Coordinates Track -->
      <circle cx="180" cy="180" r="135" stroke="rgba(255, 255, 255, 0.08)" stroke-width="1.5" stroke-dasharray="4 8" />
      <circle cx="180" cy="180" r="105" stroke="rgba(129, 140, 248, 0.15)" stroke-width="1" />

      <!-- Cardinal Zenith Indicators -->
      <line x1="180" y1="36" x2="180" y2="48" stroke="#818CF8" stroke-width="2.5" stroke-linecap="round" />
      <line x1="180" y1="312" x2="180" y2="324" stroke="rgba(255, 255, 255, 0.2)" stroke-width="2" stroke-linecap="round" />
      <line x1="36" y1="180" x2="48" y2="180" stroke="rgba(255, 255, 255, 0.2)" stroke-width="2" stroke-linecap="round" />
      <line x1="312" y1="180" x2="324" y2="180" stroke="rgba(255, 255, 255, 0.2)" stroke-width="2" stroke-linecap="round" />

      <!-- Dynamic Island Pill Body (Morphing Orbital Shield) -->
      <g filter="url(#glowBlue)">
        <rect x="75" y="145" width="210" height="70" rx="35" fill="url(#pillGrad)" stroke="url(#orbitGrad)" stroke-width="2" />
      </g>

      <!-- Elliptical Orbital Ring (Planetary Ring / Zenith Orbit) -->
      <ellipse cx="180" cy="180" rx="145" ry="55" transform="rotate(-24 180 180)" stroke="url(#orbitGrad)" stroke-width="2" stroke-dasharray="320 80" stroke-linecap="round" />

      <!-- The Celestial Eye / Sentinel Core -->
      <circle cx="180" cy="180" r="26" fill="url(#sentinelCore)" />
      <circle cx="180" cy="180" r="12" fill="#FFFFFF" filter="url(#glowBlue)" />
      <circle cx="180" cy="180" r="5" fill="#1E1B4B" />

      <!-- Satellite Orbit Nodes (Floating Dot particles) -->
      <circle cx="95" cy="180" r="4.5" fill="#38BDF8" filter="url(#glowBlue)" />
      <circle cx="265" cy="180" r="4.5" fill="#818CF8" filter="url(#glowBlue)" />

      <!-- Star flare glint -->
      <line x1="180" y1="160" x2="180" y2="200" stroke="rgba(255, 255, 255, 0.8)" stroke-width="1.5" stroke-linecap="round" />
      <line x1="160" y1="180" x2="200" y2="180" stroke="rgba(255, 255, 255, 0.8)" stroke-width="1.5" stroke-linecap="round" />
    </svg>

    <div class="label-group">
      <div class="title">The Celestial Sentinel</div>
      <div class="subtitle">Orbital Eye & Cosmic Zenith</div>
    </div>
  </div>
</body>
</html>
`;

// 3. THE SWISS BUTLER (Precision Artisan / Dieter Rams Monolith)
const htmlSwiss = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 1024px; height: 1024px;
    background: #0B0C0E;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    font-family: "JetBrains Mono", -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #fff;
    overflow: hidden;
    position: relative;
  }
  .bg-glow {
    position: absolute;
    width: 600px; height: 600px;
    background: radial-gradient(circle, rgba(245, 158, 11, 0.08) 0%, transparent 60%);
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    pointer-events: none;
  }
  .card {
    width: 720px; height: 720px;
    background: #14161B;
    border-radius: 160px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    box-shadow: 
      0 30px 80px rgba(0, 0, 0, 0.85),
      inset 0 1px 1px rgba(255, 255, 255, 0.12),
      inset 0 -1px 3px rgba(0, 0, 0, 0.6);
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    position: relative;
    padding: 40px;
  }
  .badge-tag {
    position: absolute;
    top: 52px;
    padding: 6px 16px;
    background: rgba(245, 158, 11, 0.1);
    border: 1px solid rgba(245, 158, 11, 0.25);
    border-radius: 999px;
    font-size: 13px;
    letter-spacing: 2px;
    font-weight: 600;
    color: #F59E0B;
    text-transform: uppercase;
  }
  .label-group {
    margin-top: 48px;
    text-align: center;
  }
  .title {
    font-size: 32px;
    font-weight: 700;
    letter-spacing: 4px;
    color: #FFFFFF;
    text-transform: uppercase;
  }
  .subtitle {
    margin-top: 8px;
    font-size: 14px;
    font-weight: 500;
    letter-spacing: 3px;
    color: #71717A;
    text-transform: uppercase;
  }
</style>
</head>
<body>
  <div class="bg-glow"></div>
  <div class="card">
    <div class="badge-tag">Concept 03 // Precision</div>
    
    <svg width="360" height="360" viewBox="0 0 360 360" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id="monolithGrad" x1="50" y1="120" x2="310" y2="240" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="#242831" />
          <stop offset="50%" stop-color="#1A1D24" />
          <stop offset="100%" stop-color="#101217" />
        </linearGradient>

        <linearGradient id="amberGrad" x1="0" y1="0" x2="1" y2="1">
          <stop offset="0%" stop-color="#FBBF24" />
          <stop offset="100%" stop-color="#D97706" />
        </linearGradient>

        <filter id="amberGlow" x="-20%" y="-20%" width="140%" height="140%">
          <feGaussianBlur stdDeviation="8" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      <!-- Caliper Grid & Measurement Hashes -->
      <g stroke="rgba(255, 255, 255, 0.15)" stroke-width="1.5">
        <line x1="55" y1="100" x2="305" y2="100" />
        <!-- Ticks -->
        <line x1="55" y1="92" x2="55" y2="100" />
        <line x1="105" y1="95" x2="105" y2="100" />
        <line x1="155" y1="95" x2="155" y2="100" />
        <line x1="180" y1="90" x2="180" y2="100" stroke="#F59E0B" stroke-width="2" />
        <line x1="205" y1="95" x2="205" y2="100" />
        <line x1="255" y1="95" x2="255" y2="100" />
        <line x1="305" y1="92" x2="305" y2="100" />
      </g>

      <!-- The Precision Monolith Pill (Tactile Chamfered Body) -->
      <rect x="55" y="125" width="250" height="110" rx="55" fill="url(#monolithGrad)" stroke="rgba(255,255,255,0.14)" stroke-width="2" />
      
      <!-- Inner milled groove -->
      <rect x="65" y="135" width="230" height="90" rx="45" fill="#0C0D10" stroke="rgba(0,0,0,0.8)" stroke-width="2" />

      <!-- Left Status / Track indicator (Dieter Rams Volume Wheel slot) -->
      <circle cx="110" cy="180" r="26" fill="#181B22" stroke="rgba(255,255,255,0.08)" stroke-width="1.5" />
      <circle cx="110" cy="180" r="10" fill="#2A2F3B" />
      <rect x="108" y="158" width="4" height="8" rx="2" fill="rgba(255,255,255,0.4)" />

      <!-- The Signature Zenith Dot (Braun Signal Amber) -->
      <g filter="url(#amberGlow)">
        <circle cx="215" cy="180" r="24" fill="url(#amberGrad)" />
        <circle cx="215" cy="180" r="24" stroke="#FDE68A" stroke-width="2" />
        <circle cx="215" cy="180" r="6" fill="#FFFFFF" />
      </g>

      <!-- Center separator seam -->
      <line x1="162" y1="150" x2="162" y2="210" stroke="rgba(255,255,255,0.06)" stroke-width="2" stroke-dasharray="4 4" />

      <!-- Technical Micro Typography inside SVG -->
      <text x="180" y="275" font-family="'JetBrains Mono', monospace" font-size="11" font-weight="600" fill="#71717A" text-anchor="middle" letter-spacing="3">C++20 // D2D // NATIVE 60FPS</text>
    </svg>

    <div class="label-group">
      <div class="title">The Swiss Monolith</div>
      <div class="subtitle">Braun-Inspired Precision Artisan</div>
    </div>
  </div>
</body>
</html>
`;

// 4. THE NANO-PROBE (Tactical AI Drone / Kuro Dot-01)
const htmlProbe = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 1024px; height: 1024px;
    background: #06080A;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #fff;
    overflow: hidden;
    position: relative;
  }
  .bg-glow {
    position: absolute;
    width: 650px; height: 650px;
    background: radial-gradient(circle, rgba(6, 182, 212, 0.12) 0%, transparent 65%);
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    pointer-events: none;
  }
  .card {
    width: 720px; height: 720px;
    background: linear-gradient(145deg, #0D1217 0%, #06080B 100%);
    border-radius: 160px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    box-shadow: 
      0 30px 80px rgba(0, 0, 0, 0.85),
      0 0 0 1px rgba(6, 182, 212, 0.18),
      inset 0 1px 2px rgba(255, 255, 255, 0.14),
      inset 0 -2px 6px rgba(0, 0, 0, 0.6);
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    position: relative;
    padding: 40px;
  }
  .badge-tag {
    position: absolute;
    top: 52px;
    padding: 6px 16px;
    background: rgba(6, 182, 212, 0.12);
    border: 1px solid rgba(6, 182, 212, 0.3);
    border-radius: 999px;
    font-size: 13px;
    letter-spacing: 2px;
    font-weight: 600;
    color: #22D3EE;
    text-transform: uppercase;
  }
  .label-group {
    margin-top: 48px;
    text-align: center;
  }
  .title {
    font-size: 34px;
    font-weight: 700;
    letter-spacing: 4px;
    color: #FFFFFF;
    text-transform: uppercase;
  }
  .subtitle {
    margin-top: 8px;
    font-size: 15px;
    font-weight: 500;
    letter-spacing: 3px;
    color: #94A3B8;
    text-transform: uppercase;
  }
</style>
</head>
<body>
  <div class="bg-glow"></div>
  <div class="card">
    <div class="badge-tag">Concept 04 // Cyberpunk</div>
    
    <svg width="360" height="360" viewBox="0 0 360 360" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <linearGradient id="droneGrad" x1="60" y1="80" x2="300" y2="280" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="#1B242D" />
          <stop offset="50%" stop-color="#0E141A" />
          <stop offset="100%" stop-color="#07090C" />
        </linearGradient>

        <linearGradient id="cyanNeon" x1="0" y1="0" x2="1" y2="0">
          <stop offset="0%" stop-color="#06B6D4" />
          <stop offset="50%" stop-color="#67E8F9" />
          <stop offset="100%" stop-color="#06B6D4" />
        </linearGradient>

        <filter id="cyanGlow" x="-30%" y="-30%" width="160%" height="160%">
          <feGaussianBlur stdDeviation="8" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      <!-- HUD Reticle & Crosshair Corners -->
      <g stroke="#0891B2" stroke-width="1.5">
        <!-- Top Left -->
        <path d="M 60 85 L 60 65 L 85 65" fill="none" />
        <!-- Top Right -->
        <path d="M 300 85 L 300 65 L 275 65" fill="none" />
        <!-- Bottom Left -->
        <path d="M 60 275 L 60 295 L 85 295" fill="none" />
        <!-- Bottom Right -->
        <path d="M 300 275 L 300 295 L 275 295" fill="none" />
      </g>

      <!-- Docking Wing Clamps -->
      <path d="M 45 180 L 70 145 L 85 145 L 65 180 L 85 215 L 70 215 Z" fill="#151C24" stroke="rgba(6, 182, 212, 0.4)" stroke-width="1.5" />
      <path d="M 315 180 L 290 145 L 275 145 L 295 180 L 275 215 L 290 215 Z" fill="#151C24" stroke="rgba(6, 182, 212, 0.4)" stroke-width="1.5" />

      <!-- Drone Faceted Chassis -->
      <path d="
        M 105 130 
        L 255 130 
        L 285 180 
        L 255 230 
        L 105 230 
        L 75 180 Z
      " fill="url(#droneGrad)" stroke="rgba(255,255,255,0.15)" stroke-width="2" />

      <!-- Visor Optical Chamber (Recessed) -->
      <rect x="100" y="160" width="160" height="40" rx="8" fill="#040608" stroke="rgba(6, 182, 212, 0.3)" stroke-width="1.5" />

      <!-- Scanning Laser Beam / Optical Aperture -->
      <g filter="url(#cyanGlow)">
        <rect x="110" y="177" width="140" height="6" rx="3" fill="url(#cyanNeon)" />
        <!-- Glowing Core Drone Eye -->
        <circle cx="180" cy="180" r="14" fill="#06B6D4" />
        <circle cx="180" cy="180" r="7" fill="#FFFFFF" />
      </g>

      <!-- Sensor Pinholes / Status LEDs -->
      <circle cx="120" cy="145" r="3" fill="#22D3EE" filter="url(#cyanGlow)" />
      <circle cx="132" cy="145" r="3" fill="rgba(255,255,255,0.2)" />
      <circle cx="240" cy="145" r="3" fill="#22D3EE" filter="url(#cyanGlow)" />
      <circle cx="228" cy="145" r="3" fill="rgba(255,255,255,0.2)" />

      <!-- Tactical Marking -->
      <text x="180" y="255" font-family="'JetBrains Mono', monospace" font-size="10" font-weight="700" fill="#0891B2" text-anchor="middle" letter-spacing="4">SYS.PROBE // KURO-01</text>
    </svg>

    <div class="label-group">
      <div class="title">The Nano-Probe</div>
      <div class="subtitle">Cyberpunk Tactical Drone // Kuro</div>
    </div>
  </div>
</body>
</html>
`;

// 5. SHOWCASE COMPARISON BOARD (2400x1500)
const htmlShowcase = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 2400px; height: 1500px;
    background: #08090C;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #fff;
    display: flex; flex-direction: column;
    padding: 80px 100px;
    overflow: hidden;
  }
  .header {
    display: flex; justify-content: space-between; align-items: flex-end;
    margin-bottom: 60px;
    border-bottom: 1px solid rgba(255, 255, 255, 0.08);
    padding-bottom: 30px;
  }
  .brand-title {
    font-size: 52px; font-weight: 800; letter-spacing: 6px;
    background: linear-gradient(180deg, #FFFFFF 0%, #A1A1AA 100%);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .brand-sub {
    font-size: 20px; font-weight: 500; color: #71717A; letter-spacing: 3px; margin-top: 8px;
  }
  .meta-tag {
    font-family: "JetBrains Mono", monospace;
    font-size: 16px; color: #10B981; letter-spacing: 2px;
    background: rgba(16, 185, 129, 0.1);
    border: 1px solid rgba(16, 185, 129, 0.2);
    padding: 10px 24px; border-radius: 999px;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 40px;
    flex: 1;
  }
  .col-card {
    background: linear-gradient(160deg, #12141A 0%, #0A0B0E 100%);
    border-radius: 40px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    box-shadow: 0 20px 50px rgba(0,0,0,0.6);
    display: flex; flex-direction: column;
    padding: 36px 30px;
    position: relative;
  }
  .col-badge {
    align-self: flex-start;
    padding: 6px 14px;
    border-radius: 999px;
    font-size: 12px;
    font-weight: 700;
    letter-spacing: 1.5px;
    text-transform: uppercase;
    margin-bottom: 24px;
  }
  .badge-1 { background: rgba(16, 185, 129, 0.15); color: #34D399; border: 1px solid rgba(16, 185, 129, 0.3); }
  .badge-2 { background: rgba(99, 102, 241, 0.15); color: #818CF8; border: 1px solid rgba(99, 102, 241, 0.3); }
  .badge-3 { background: rgba(245, 158, 11, 0.15); color: #F59E0B; border: 1px solid rgba(245, 158, 11, 0.3); }
  .badge-4 { background: rgba(6, 182, 212, 0.15); color: #22D3EE; border: 1px solid rgba(6, 182, 212, 0.3); }

  .icon-stage {
    flex: 1;
    display: flex; align-items: center; justify-content: center;
  }
  .card-footer {
    border-top: 1px solid rgba(255, 255, 255, 0.06);
    padding-top: 24px;
    margin-top: 20px;
  }
  .col-title {
    font-size: 24px; font-weight: 700; letter-spacing: 2px; color: #FFF;
  }
  .col-sub {
    font-size: 14px; color: #94A3B8; margin-top: 6px; letter-spacing: 1px;
  }
  .col-desc {
    font-size: 14px; color: #64748B; margin-top: 14px; line-height: 1.6;
  }
</style>
</head>
<body>
  <div class="header">
    <div>
      <div class="brand-title">ZENITH DOT · IDENTITY SUITE</div>
      <div class="brand-sub">Native Windows 11 Dynamic Island // Visual Persona Exploration</div>
    </div>
    <div class="meta-tag">C++20 · DIRECT2D · ZERO-DEPENDENCY</div>
  </div>

  <div class="grid">
    <!-- Card 1 -->
    <div class="col-card">
      <div class="col-badge badge-1">Option 01 // Companion</div>
      <div class="icon-stage">
        <!-- Embedded Cat SVG scaled -->
        <svg width="280" height="280" viewBox="0 0 360 360" fill="none">
          <path d="M 85 180 C 85 125, 125 105, 120 70 C 118 55, 138 60, 155 85 C 165 95, 195 95, 205 85 C 222 60, 242 55, 240 70 C 235 105, 275 125, 275 180 C 275 235, 235 260, 180 260 C 125 260, 85 235, 85 180 Z" fill="#0C0E12" stroke="#34D399" stroke-width="2.5" />
          <path d="M 132 175 Q 150 162 162 175 Q 150 184 132 175 Z" fill="#34D399" />
          <ellipse cx="147" cy="174" rx="2" ry="4" fill="#042F2E" />
          <path d="M 198 175 Q 210 162 228 175 Q 210 184 198 175 Z" fill="#34D399" />
          <ellipse cx="213" cy="174" rx="2" ry="4" fill="#042F2E" />
          <circle cx="180" cy="132" r="7" fill="#34D399" />
          <circle cx="180" cy="132" r="2.5" fill="#FFFFFF" />
          <line x1="70" y1="172" x2="98" y2="172" stroke="rgba(52, 211, 153, 0.4)" stroke-width="2" />
          <line x1="290" y1="172" x2="262" y2="172" stroke="rgba(52, 211, 153, 0.4)" stroke-width="2" />
        </svg>
      </div>
      <div class="card-footer">
        <div class="col-title">Obsidian Familiar</div>
        <div class="col-sub">Ferrofluid Cyber-Cat</div>
        <div class="col-desc">Kucing siluet monokrom yang tidur di notch bezel atas. Cairan ferrofluid lentur saat memutar lagu atau menerima drop file.</div>
      </div>
    </div>

    <!-- Card 2 -->
    <div class="col-card">
      <div class="col-badge badge-2">Option 02 // Astronomy</div>
      <div class="icon-stage">
        <svg width="280" height="280" viewBox="0 0 360 360" fill="none">
          <circle cx="180" cy="180" r="110" stroke="rgba(129, 140, 248, 0.2)" stroke-width="1.5" stroke-dasharray="4 8" />
          <rect x="75" y="145" width="210" height="70" rx="35" fill="#0A0B16" stroke="#818CF8" stroke-width="2" />
          <ellipse cx="180" cy="180" rx="135" ry="50" transform="rotate(-24 180 180)" stroke="#38BDF8" stroke-width="2" stroke-linecap="round" />
          <circle cx="180" cy="180" r="22" fill="#818CF8" />
          <circle cx="180" cy="180" r="9" fill="#FFFFFF" />
          <circle cx="95" cy="180" r="4.5" fill="#38BDF8" />
          <circle cx="265" cy="180" r="4.5" fill="#818CF8" />
        </svg>
      </div>
      <div class="card-footer">
        <div class="col-title">Celestial Sentinel</div>
        <div class="col-sub">Orbital Zenith Eye</div>
        <div class="col-desc">Representasi astronomi dari titik Zenith tertinggi langit. Lensa orbital presisi tinggi yang senyap mengawasi sistem.</div>
      </div>
    </div>

    <!-- Card 3 -->
    <div class="col-card">
      <div class="col-badge badge-3">Option 03 // Precision</div>
      <div class="icon-stage">
        <svg width="280" height="280" viewBox="0 0 360 360" fill="none">
          <line x1="55" y1="110" x2="305" y2="110" stroke="rgba(255, 255, 255, 0.2)" stroke-width="1.5" />
          <line x1="180" y1="102" x2="180" y2="110" stroke="#F59E0B" stroke-width="2" />
          <rect x="55" y="130" width="250" height="100" rx="50" fill="#15171D" stroke="rgba(255,255,255,0.15)" stroke-width="2" />
          <rect x="65" y="140" width="230" height="80" rx="40" fill="#0C0D10" />
          <circle cx="115" cy="180" r="20" fill="#242831" />
          <circle cx="215" cy="180" r="22" fill="#F59E0B" />
          <circle cx="215" cy="180" r="6" fill="#FFFFFF" />
        </svg>
      </div>
      <div class="card-footer">
        <div class="col-title">Swiss Monolith</div>
        <div class="col-sub">Dieter Rams & Braun Style</div>
        <div class="col-desc">Filosofi desain industri Jerman/Swiss. Chamfered tactile pill dengan signature Braun Signal Amber Dot. Zero-clutter mutlak.</div>
      </div>
    </div>

    <!-- Card 4 -->
    <div class="col-card">
      <div class="col-badge badge-4">Option 04 // Cyberpunk</div>
      <div class="icon-stage">
        <svg width="280" height="280" viewBox="0 0 360 360" fill="none">
          <g stroke="#0891B2" stroke-width="1.5">
            <path d="M 65 85 L 65 65 L 85 65" fill="none" />
            <path d="M 295 85 L 295 65 L 275 65" fill="none" />
            <path d="M 65 275 L 65 295 L 85 295" fill="none" />
            <path d="M 295 275 L 295 295 L 275 295" fill="none" />
          </g>
          <path d="M 105 135 L 255 135 L 285 180 L 255 225 L 105 225 L 75 180 Z" fill="#0E141A" stroke="#22D3EE" stroke-width="2" />
          <rect x="110" y="165" width="140" height="30" rx="6" fill="#040608" stroke="#0891B2" />
          <rect x="120" y="177" width="120" height="6" rx="3" fill="#22D3EE" />
          <circle cx="180" cy="180" r="10" fill="#22D3EE" />
          <circle cx="180" cy="180" r="4" fill="#FFFFFF" />
        </svg>
      </div>
      <div class="card-footer">
        <div class="col-title">Nano-Probe</div>
        <div class="col-sub">Tactical Drone // Kuro-01</div>
        <div class="col-desc">Drone taktis mini bersayap docking clip dengan laser visor cyan. Cocok untuk visual cyberpunk HUD dan monitoring sistem.</div>
      </div>
    </div>
  </div>
</body>
</html>
`;

async function renderAll() {
  const browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });
  
  const tasks = [
    { name: 'zenith-cat-familiar.png', html: htmlCat, width: 1024, height: 1024, scale: 2 },
    { name: 'zenith-celestial-sentinel.png', html: htmlSentinel, width: 1024, height: 1024, scale: 2 },
    { name: 'zenith-swiss-monolith.png', html: htmlSwiss, width: 1024, height: 1024, scale: 2 },
    { name: 'zenith-tactile-nanoprobe.png', html: htmlProbe, width: 1024, height: 1024, scale: 2 },
    { name: 'zenith-logo-showcase-all.png', html: htmlShowcase, width: 2400, height: 1500, scale: 1.5 },
  ];

  for (const t of tasks) {
    const page = await browser.newPage({ viewport: { width: t.width, height: t.height, deviceScaleFactor: t.scale } });
    await page.setContent(t.html, { waitUntil: 'load' });
    const outPath = path.join(OUTPUT_DIR, t.name);
    await page.screenshot({ path: outPath });
    console.log(`Rendered: ${outPath} (${t.width * t.scale}x${t.height * t.scale})`);
    await page.close();
  }

  await browser.close();
  console.log('ALL LOGOS RENDERED SUCCESSFULLY!');
}

renderAll().catch(err => {
  console.error(err);
  process.exit(1);
});
