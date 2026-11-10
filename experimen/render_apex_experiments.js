const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const OUTPUT_DIR = '/home/alifihsan/Zenith-Dot/experimen/logos';
if (!fs.existsSync(OUTPUT_DIR)) {
  fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

// 1. VARIATION 1: MERIDIAN ARCH (The Pure Refined Celestial Apex)
const svgApex1 = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="bodyGrad1" x1="60" y1="120" x2="340" y2="280" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#1B2536" />
      <stop offset="45%" stop-color="#0E1522" />
      <stop offset="100%" stop-color="#06090F" />
    </linearGradient>
    <linearGradient id="rimGrad1" x1="60" y1="80" x2="340" y2="300" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.7" />
      <stop offset="25%" stop-color="#91CAFF" />
      <stop offset="60%" stop-color="rgba(145, 202, 255, 0.15)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.3" />
    </linearGradient>
    <linearGradient id="beamGrad1" x1="200" y1="70" x2="200" y2="180" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#91CAFF" stop-opacity="0.7" />
      <stop offset="100%" stop-color="#0284C7" stop-opacity="0" />
    </linearGradient>
    <radialGradient id="dotGlow1" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#FFFFFF" />
      <stop offset="35%" stop-color="#91CAFF" />
      <stop offset="70%" stop-color="#0284C7" stop-opacity="0.4" />
      <stop offset="100%" stop-color="#0369A1" stop-opacity="0" />
    </radialGradient>
    <filter id="shadow1" x="-25%" y="-25%" width="150%" height="150%">
      <feDropShadow dx="0" dy="24" stdDeviation="28" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="10" flood-color="#0C1422" flood-opacity="0.8" />
    </filter>
    <filter id="glow1" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Astronomical Meridian Grid Line -->
  <line x1="200" y1="45" x2="200" y2="355" stroke="rgba(145, 202, 255, 0.1)" stroke-width="1.5" stroke-dasharray="4 8" />
  <circle cx="200" cy="200" r="145" stroke="rgba(145, 202, 255, 0.08)" stroke-width="1.5" />

  <!-- Dynamic Island Body with Upward Apex Arch -->
  <g filter="url(#shadow1)">
    <path d="
      M 75 200
      C 75 160, 110 160, 140 160
      C 165 160, 182 132, 200 112
      C 218 132, 235 160, 260 160
      C 290 160, 325 160, 325 200
      C 325 240, 290 240, 260 240
      C 235 240, 218 240, 200 240
      C 182 240, 165 240, 140 240
      C 110 240, 75 240, 75 200 Z
    " fill="url(#bodyGrad1)" stroke="url(#rimGrad1)" stroke-width="2.8" />

    <!-- Specular rim highlight on the apex crest -->
    <path d="M 175 142 C 185 126, 193 114, 200 114 C 207 114, 215 126, 225 142" stroke="#FFFFFF" stroke-width="2" stroke-linecap="round" fill="none" opacity="0.8" />
  </g>

  <!-- Zenith Apex Projection Beam -->
  <path d="M 188 122 L 200 86 L 212 122 Z" fill="url(#beamGrad1)" />

  <!-- Suspended Polaris Zenith Dot with Aura -->
  <g filter="url(#glow1)">
    <circle cx="200" cy="86" r="18" fill="url(#dotGlow1)" />
    <circle cx="200" cy="86" r="8" fill="#91CAFF" />
    <circle cx="200" cy="86" r="3" fill="#FFFFFF" />
  </g>

  <!-- Inner Status Slot / Sensor Aperture -->
  <rect x="120" y="194" width="160" height="12" rx="6" fill="#0A0E16" stroke="rgba(145, 202, 255, 0.15)" stroke-width="1" />
  <circle cx="132" cy="200" r="3.5" fill="#38BDF8" />
  <circle cx="268" cy="200" r="3.5" fill="#91CAFF" />
</svg>
`;

// 2. VARIATION 2: PRISM FACET (Precision Chamfer / Optical Crystal Bevel)
const svgApex2 = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="bodyGrad2" x1="60" y1="120" x2="340" y2="280" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#1B2536" />
      <stop offset="50%" stop-color="#0E1420" />
      <stop offset="100%" stop-color="#06090F" />
    </linearGradient>
    <linearGradient id="rimGrad2" x1="70" y1="80" x2="330" y2="300" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.75" />
      <stop offset="30%" stop-color="#91CAFF" />
      <stop offset="70%" stop-color="rgba(145, 202, 255, 0.2)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.3" />
    </linearGradient>
    <filter id="shadow2" x="-25%" y="-25%" width="150%" height="150%">
      <feDropShadow dx="0" dy="24" stdDeviation="28" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="10" flood-color="#0C1422" flood-opacity="0.8" />
    </filter>
    <filter id="glow2" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Hexagonal / Chamfered Construction Ring -->
  <polygon points="200,60 320,130 320,270 200,340 80,270 80,130" stroke="rgba(145, 202, 255, 0.07)" stroke-width="1.5" fill="none" stroke-dasharray="6 8" />

  <!-- Pill with Faceted 45-degree Chamfered Apex -->
  <g filter="url(#shadow2)">
    <!-- Body -->
    <path d="
      M 70 200
      C 70 162, 105 162, 135 162
      L 170 162
      L 200 110
      L 230 162
      L 265 162
      C 295 162, 330 162, 330 200
      C 330 238, 295 238, 265 238
      L 135 238
      C 105 238, 70 238, 70 200 Z
    " fill="url(#bodyGrad2)" stroke="url(#rimGrad2)" stroke-width="2.8" stroke-linejoin="round" />

    <!-- Faceted Bevel Polygons on the Apex -->
    <!-- Left Chamfer Plane -->
    <polygon points="170,162 200,110 200,162" fill="#182232" stroke="rgba(145, 202, 255, 0.2)" stroke-width="1" />
    <!-- Right Chamfer Plane -->
    <polygon points="230,162 200,110 200,162" fill="#0F1622" stroke="rgba(145, 202, 255, 0.15)" stroke-width="1" />
  </g>

  <!-- Laser Core Floating Above the Prism Peak -->
  <g filter="url(#glow2)">
    <!-- Optical Refraction Ring -->
    <circle cx="200" cy="80" r="16" stroke="rgba(145, 202, 255, 0.3)" stroke-width="1.5" stroke-dasharray="2 4" />
    <circle cx="200" cy="80" r="8" fill="#91CAFF" />
    <circle cx="200" cy="80" r="3" fill="#FFFFFF" />
    <!-- Vertical Spark -->
    <line x1="200" y1="62" x2="200" y2="98" stroke="rgba(255, 255, 255, 0.8)" stroke-width="1.5" stroke-linecap="round" />
    <line x1="182" y1="80" x2="218" y2="80" stroke="rgba(255, 255, 255, 0.8)" stroke-width="1.5" stroke-linecap="round" />
  </g>

  <!-- Precision Caliper Ticks on Base -->
  <line x1="110" y1="200" x2="110" y2="212" stroke="rgba(145, 202, 255, 0.3)" stroke-width="1.5" stroke-linecap="round" />
  <line x1="150" y1="200" x2="150" y2="208" stroke="rgba(145, 202, 255, 0.2)" stroke-width="1.5" stroke-linecap="round" />
  <line x1="250" y1="200" x2="250" y2="208" stroke="rgba(145, 202, 255, 0.2)" stroke-width="1.5" stroke-linecap="round" />
  <line x1="290" y1="200" x2="290" y2="212" stroke="rgba(145, 202, 255, 0.3)" stroke-width="1.5" stroke-linecap="round" />
</svg>
`;

// 3. VARIATION 3: FLUSH BEZEL APEX (Inverted Notch Anchor / Screen-Flush)
const svgApex3 = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="bodyGrad3" x1="60" y1="80" x2="340" y2="260" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#192230" />
      <stop offset="50%" stop-color="#0E1420" />
      <stop offset="100%" stop-color="#06090E" />
    </linearGradient>
    <linearGradient id="rimGrad3" x1="60" y1="80" x2="340" y2="260" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.8" />
      <stop offset="25%" stop-color="#91CAFF" />
      <stop offset="65%" stop-color="rgba(145, 202, 255, 0.2)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.4" />
    </linearGradient>
    <filter id="shadow3" x="-25%" y="-25%" width="150%" height="150%">
      <feDropShadow dx="0" dy="24" stdDeviation="28" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="10" flood-color="#0C1422" flood-opacity="0.8" />
    </filter>
    <filter id="glow3" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Top Display Bezel Anchor Line -->
  <line x1="40" y1="90" x2="360" y2="90" stroke="rgba(255, 255, 255, 0.25)" stroke-width="2.5" stroke-linecap="round" />

  <!-- Bezel Notch Hanging Down with Upward Inverted Apex Saddle -->
  <g filter="url(#shadow3)">
    <path d="
      M 70 90
      L 130 90
      C 155 90, 168 114, 185 114
      C 192 114, 196 110, 200 102
      C 204 110, 208 114, 215 114
      C 232 114, 245 90, 270 90
      L 330 90
      C 330 140, 330 180, 330 200
      C 330 250, 270 250, 200 250
      C 130 250, 70 250, 70 200
      C 70 180, 70 140, 70 90 Z
    " fill="url(#bodyGrad3)" stroke="url(#rimGrad3)" stroke-width="2.8" stroke-linejoin="round" />
  </g>

  <!-- Suspended Core Dot in the Apex Saddle -->
  <g filter="url(#glow3)">
    <circle cx="200" cy="74" r="16" fill="rgba(145, 202, 255, 0.25)" />
    <circle cx="200" cy="74" r="9" fill="#91CAFF" />
    <circle cx="200" cy="74" r="3.5" fill="#FFFFFF" />
  </g>

  <!-- Status Capsule Readout Inside -->
  <rect x="110" y="180" width="180" height="34" rx="17" fill="#0A0E16" stroke="rgba(145, 202, 255, 0.15)" stroke-width="1.5" />
  <circle cx="132" cy="197" r="5" fill="#38BDF8" filter="url(#glow3)" />
  <line x1="150" y1="197" x2="260" y2="197" stroke="rgba(145, 202, 255, 0.3)" stroke-width="2" stroke-linecap="round" />
</svg>
`;

// 4. VARIATION 4: ORBITAL RESONANCE (Concentric Wavefronts Apex)
const svgApex4 = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="bodyGrad4" x1="60" y1="120" x2="340" y2="280" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#1C2638" />
      <stop offset="45%" stop-color="#0E1522" />
      <stop offset="100%" stop-color="#06090F" />
    </linearGradient>
    <linearGradient id="rimGrad4" x1="60" y1="80" x2="340" y2="300" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.7" />
      <stop offset="25%" stop-color="#91CAFF" />
      <stop offset="60%" stop-color="rgba(145, 202, 255, 0.15)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.3" />
    </linearGradient>
    <filter id="shadow4" x="-25%" y="-25%" width="150%" height="150%">
      <feDropShadow dx="0" dy="24" stdDeviation="28" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="10" flood-color="#0C1422" flood-opacity="0.8" />
    </filter>
    <filter id="glow4" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Triple Concentric Resonance Wavefronts Emerging From Apex -->
  <path d="M 140 100 C 160 85, 240 85, 260 100" stroke="#91CAFF" stroke-width="1.5" stroke-dasharray="4 6" fill="none" opacity="0.4" />
  <path d="M 155 82 C 170 70, 230 70, 245 82" stroke="#91CAFF" stroke-width="1.8" fill="none" opacity="0.6" />
  <path d="M 172 66 C 182 58, 218 58, 228 66" stroke="#91CAFF" stroke-width="2" fill="none" opacity="0.85" />

  <!-- Dynamic Island Body with Upward Apex Arch -->
  <g filter="url(#shadow4)">
    <path d="
      M 75 200
      C 75 160, 110 160, 140 160
      C 165 160, 182 135, 200 115
      C 218 135, 235 160, 260 160
      C 290 160, 325 160, 325 200
      C 325 240, 290 240, 260 240
      C 235 240, 218 240, 200 240
      C 182 240, 165 240, 140 240
      C 110 240, 75 240, 75 200 Z
    " fill="url(#bodyGrad4)" stroke="url(#rimGrad4)" stroke-width="2.8" />
  </g>

  <!-- Zenith Focal Core Dot in Orbit -->
  <g filter="url(#glow4)">
    <circle cx="200" cy="50" r="14" fill="rgba(145, 202, 255, 0.3)" />
    <circle cx="200" cy="50" r="7.5" fill="#91CAFF" />
    <circle cx="200" cy="50" r="3" fill="#FFFFFF" />
  </g>

  <!-- Soundwave / Equalizer Bars In Island Center -->
  <g opacity="0.8" stroke-linecap="round">
    <line x1="170" y1="192" x2="170" y2="208" stroke="#38BDF8" stroke-width="2.5" />
    <line x1="180" y1="184" x2="180" y2="216" stroke="#91CAFF" stroke-width="2.5" />
    <line x1="190" y1="188" x2="190" y2="212" stroke="#91CAFF" stroke-width="2.5" />
    <line x1="200" y1="180" x2="200" y2="220" stroke="#FFFFFF" stroke-width="2.5" />
    <line x1="210" y1="186" x2="210" y2="214" stroke="#91CAFF" stroke-width="2.5" />
    <line x1="220" y1="182" x2="220" y2="218" stroke="#91CAFF" stroke-width="2.5" />
    <line x1="230" y1="194" x2="230" y2="206" stroke="#38BDF8" stroke-width="2.5" />
  </g>
</svg>
`;

function buildCardHtml(title, subtitle, badge, svgContent) {
  return `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 1024px; height: 1024px;
    background: #05070B;
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #fff;
    overflow: hidden;
    position: relative;
  }
  .bg-radial {
    position: absolute;
    width: 800px; height: 800px;
    background: radial-gradient(circle, rgba(145, 202, 255, 0.12) 0%, rgba(12, 20, 34, 0.4) 40%, transparent 70%);
    top: 50%; left: 50%;
    transform: translate(-50%, -50%);
    pointer-events: none;
  }
  .squircle {
    width: 740px; height: 740px;
    background: linear-gradient(155deg, #131A26 0%, #0C1422 40%, #06090F 100%);
    border-radius: 170px;
    border: 1px solid rgba(145, 202, 255, 0.22);
    box-shadow: 
      0 40px 100px rgba(0, 0, 0, 0.9),
      0 0 60px rgba(145, 202, 255, 0.08),
      inset 0 1px 2px rgba(255, 255, 255, 0.2),
      inset 0 -2px 8px rgba(0, 0, 0, 0.7);
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    position: relative;
    padding: 40px;
  }
  .badge {
    position: absolute; top: 50px;
    padding: 6px 18px; background: rgba(145, 202, 255, 0.1);
    border: 1px solid rgba(145, 202, 255, 0.25); border-radius: 999px;
    font-size: 13px; font-weight: 700; color: #91CAFF; letter-spacing: 2.5px; text-transform: uppercase;
  }
  .label-group {
    margin-top: 32px;
    text-align: center;
  }
  .title {
    font-size: 34px; font-weight: 800; letter-spacing: 5px; color: #F5F6F8; text-transform: uppercase;
  }
  .subtitle {
    margin-top: 8px; font-size: 14px; font-weight: 600; letter-spacing: 3px; color: #91CAFF; text-transform: uppercase;
  }
</style>
</head>
<body>
  <div class="bg-radial"></div>
  <div class="squircle">
    <div class="badge">${badge}</div>
    ${svgContent}
    <div class="label-group">
      <div class="title">${title}</div>
      <div class="subtitle">${subtitle}</div>
    </div>
  </div>
</body>
</html>
  `;
}

// 5. MASTER EXPERIMENTS SHOWCASE SHEET (2400x1450)
const htmlExpShowcase = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 2400px; height: 1450px;
    background: #05070B;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #F5F6F8;
    display: flex; flex-direction: column;
    padding: 70px 90px;
    overflow: hidden;
  }
  .header {
    display: flex; justify-content: space-between; align-items: flex-end;
    border-bottom: 1px solid rgba(145, 202, 255, 0.15);
    padding-bottom: 26px; margin-bottom: 50px;
  }
  .brand-title {
    font-size: 46px; font-weight: 800; letter-spacing: 6px;
    background: linear-gradient(180deg, #FFFFFF 0%, #91CAFF 100%);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .brand-sub {
    font-size: 18px; font-weight: 500; color: #94A3B8; letter-spacing: 3px; margin-top: 6px;
  }
  .meta-tag {
    font-family: "JetBrains Mono", monospace; font-size: 14px; color: #91CAFF; letter-spacing: 2px;
    background: rgba(145, 202, 255, 0.1); border: 1px solid rgba(145, 202, 255, 0.25);
    padding: 8px 20px; border-radius: 999px;
  }
  .grid {
    display: grid;
    grid-template-columns: repeat(4, 1fr);
    gap: 36px;
    flex: 1;
  }
  .card {
    background: linear-gradient(160deg, #101622 0%, #080C14 100%);
    border-radius: 36px;
    border: 1px solid rgba(145, 202, 255, 0.18);
    box-shadow: 0 25px 60px rgba(0,0,0,0.7);
    display: flex; flex-direction: column;
    padding: 34px 28px;
    position: relative;
  }
  .card-badge {
    align-self: flex-start;
    padding: 6px 14px; border-radius: 999px;
    font-size: 11px; font-weight: 700; letter-spacing: 2px; text-transform: uppercase;
    background: rgba(145, 202, 255, 0.12); color: #91CAFF; border: 1px solid rgba(145, 202, 255, 0.3);
    margin-bottom: 20px;
  }
  .stage {
    flex: 1; display: flex; align-items: center; justify-content: center;
  }
  .footer {
    border-top: 1px solid rgba(255, 255, 255, 0.06);
    padding-top: 20px; margin-top: 16px;
  }
  .title { font-size: 22px; font-weight: 700; color: #FFF; letter-spacing: 1.5px; }
  .sub { font-size: 13px; font-weight: 600; color: #91CAFF; margin-top: 4px; letter-spacing: 1px; }
  .desc { font-size: 13.5px; color: #78889B; margin-top: 10px; line-height: 1.55; }
</style>
</head>
<body>
  <div class="header">
    <div>
      <div class="brand-title">HORIZON APEX · LAB EXPERIMENTS</div>
      <div class="brand-sub">Zenith Dot // Architectural Explorations of the Celestial Apex Concept</div>
    </div>
    <div class="meta-tag">DIR: /experimen/logos · HIGH-DPI ASSETS</div>
  </div>

  <div class="grid">
    <!-- Card 1 -->
    <div class="card">
      <div class="card-badge">Exp 01 // Meridian Arch</div>
      <div class="stage">
        ${svgApex1.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Meridian Arch</div>
        <div class="sub">Continuous Fluid Curvature</div>
        <div class="desc">Lengkungan organik murni yang naik anggun ke zenith meridian. Proyeksi berkas cahaya lembut menopang bintang Polaris Dot.</div>
      </div>
    </div>

    <!-- Card 2 -->
    <div class="card">
      <div class="card-badge">Exp 02 // Prism Chamfer</div>
      <div class="stage">
        ${svgApex2.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Prism Facet</div>
        <div class="sub">Precision 45° Optical Chamfer</div>
        <div class="desc">Potongan bevel chamfer 45 derajat berstandar industri presisi CAD. Memfokuskan refraksi optik ke laser core di atas puncak.</div>
      </div>
    </div>

    <!-- Card 3 -->
    <div class="card">
      <div class="card-badge">Exp 03 // Bezel Saddle</div>
      <div class="stage">
        ${svgApex3.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Bezel Saddle</div>
        <div class="sub">Top-Edge Display Anchor</div>
        <div class="desc">Menempel langsung di bezel atas layar monitor. Apex terbalik membentuk saddle anggun tempat bertenggernya Zenith Dot.</div>
      </div>
    </div>

    <!-- Card 4 -->
    <div class="card">
      <div class="card-badge">Exp 04 // Resonance</div>
      <div class="stage">
        ${svgApex4.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">Orbital Resonance</div>
        <div class="sub">Acoustic Wavefront Apex</div>
        <div class="desc">Puncak apex memancarkan gelombang resonansi konsentris 3-lapis menuju Dot, mencerminkan pemutar musik dan spektrum audio C++.</div>
      </div>
    </div>
  </div>
</body>
</html>
`;

async function renderExperiments() {
  const browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });

  const tasks = [
    {
      name: 'horizon-apex-exp1-meridian-arch.png',
      html: buildCardHtml('The Meridian Arch', 'Continuous Fluid Curvature', 'Exp 01 // Meridian', svgApex1),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'horizon-apex-exp2-prism-facet.png',
      html: buildCardHtml('The Prism Facet', 'Precision 45° Optical Chamfer', 'Exp 02 // Prism', svgApex2),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'horizon-apex-exp3-bezel-saddle.png',
      html: buildCardHtml('The Bezel Saddle', 'Top-Edge Display Anchor', 'Exp 03 // Bezel Flush', svgApex3),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'horizon-apex-exp4-orbital-resonance.png',
      html: buildCardHtml('Orbital Resonance', 'Acoustic Wavefront Apex', 'Exp 04 // Resonance', svgApex4),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'horizon-apex-master-showcase.png',
      html: htmlExpShowcase,
      w: 2400, h: 1450, s: 1.5
    }
  ];

  for (const t of tasks) {
    const page = await browser.newPage({ viewport: { width: t.w, height: t.h, deviceScaleFactor: t.s } });
    await page.setContent(t.html, { waitUntil: 'load' });
    const outPath = path.join(OUTPUT_DIR, t.name);
    await page.screenshot({ path: outPath });
    console.log('Rendered:', outPath);
    await page.close();
  }

  await browser.close();
  console.log('HORIZON APEX EXPERIMENTS RENDERED SUCCESSFULLY!');
}

renderExperiments().catch(err => {
  console.error(err);
  process.exit(1);
});
