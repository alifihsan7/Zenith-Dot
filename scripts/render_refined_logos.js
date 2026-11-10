const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const OUTPUT_DIR = '/home/alifihsan/Zenith-Dot/assets/logos';
if (!fs.existsSync(OUTPUT_DIR)) {
  fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

// 1. CONCEPT A: GEOMETRIC STEALTH PANTHER / CYBER-CAT (No cartoon doodle. Automotive/Aerospace grade faceted mark)
// Think Polestar / Lamborghini / Gitlab / Razer level geometry.
const svgCatGeometric = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="stealthGrad" x1="100" y1="80" x2="300" y2="320" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#1E2638" />
      <stop offset="40%" stop-color="#111724" />
      <stop offset="100%" stop-color="#070A10" />
    </linearGradient>
    <linearGradient id="stealthBorder" x1="80" y1="60" x2="320" y2="340" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.5" />
      <stop offset="30%" stop-color="#91CAFF" />
      <stop offset="70%" stop-color="#1E3A8A" stop-opacity="0.3" />
      <stop offset="100%" stop-color="#91CAFF" stop-opacity="0.2" />
    </linearGradient>
    <linearGradient id="blueLaser" x1="0" y1="0" x2="1" y2="0">
      <stop offset="0%" stop-color="#38BDF8" />
      <stop offset="50%" stop-color="#91CAFF" />
      <stop offset="100%" stop-color="#38BDF8" />
    </linearGradient>
    <radialGradient id="coreGlow" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#C2E0FF" />
      <stop offset="45%" stop-color="#91CAFF" stop-opacity="0.9" />
      <stop offset="75%" stop-color="#0284C7" stop-opacity="0.3" />
      <stop offset="100%" stop-color="#0369A1" stop-opacity="0" />
    </radialGradient>
    <filter id="fShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="24" stdDeviation="30" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="10" flood-color="#0C1422" flood-opacity="0.8" />
    </filter>
    <filter id="fGlow" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Ambient Construction Guide Ring -->
  <circle cx="200" cy="200" r="160" stroke="rgba(145, 202, 255, 0.08)" stroke-width="1.5" stroke-dasharray="8 12" />

  <!-- Stealth Cat Geometric Mask / Faceted Badge -->
  <g filter="url(#fShadow)">
    <!-- Outer Shield / Geometric Cat Face Silhouette (Origami/Stealth precision) -->
    <path d="
      M 200 86
      L 242 124
      L 288 88
      L 282 178
      L 314 212
      L 272 238
      L 200 304
      L 128 238
      L 86 212
      L 118 178
      L 112 88
      L 158 124
      Z
    " fill="url(#stealthGrad)" stroke="url(#stealthBorder)" stroke-width="2.5" stroke-linejoin="round" />

    <!-- Faceted Interior Shading Polygons (Giving depth like a high-end luxury emblem) -->
    <!-- Center Forehead V-Facet -->
    <path d="M 200 86 L 242 124 L 200 162 L 158 124 Z" fill="#182030" opacity="0.6" stroke="rgba(145, 202, 255, 0.15)" stroke-width="1" />
    
    <!-- Left Ear Inner Plane -->
    <path d="M 112 88 L 158 124 L 118 178 Z" fill="#0D121B" stroke="rgba(145, 202, 255, 0.12)" stroke-width="1" />
    <!-- Right Ear Inner Plane -->
    <path d="M 288 88 L 242 124 L 282 178 Z" fill="#131B27" stroke="rgba(145, 202, 255, 0.12)" stroke-width="1" />

    <!-- Lower Jaw Facets -->
    <path d="M 200 162 L 272 238 L 200 304 Z" fill="#0A0E15" opacity="0.8" stroke="rgba(145, 202, 255, 0.1)" stroke-width="1" />
    <path d="M 200 162 L 128 238 L 200 304 Z" fill="#111722" opacity="0.8" stroke="rgba(145, 202, 255, 0.1)" stroke-width="1" />
  </g>

  <!-- Visor Slit Eyes (Cybernetic Horizontal Hexagonal Apertures) -->
  <g filter="url(#fGlow)">
    <!-- Left Eye Visor -->
    <path d="M 142 198 L 174 192 L 176 202 L 146 206 Z" fill="url(#blueLaser)" />
    <!-- Right Eye Visor -->
    <path d="M 258 198 L 226 192 L 224 202 L 254 206 Z" fill="url(#blueLaser)" />
  </g>

  <!-- The Crown "Zenith Dot" (Floating focal jewel at top forehead apex) -->
  <g filter="url(#fGlow)">
    <circle cx="200" cy="144" r="14" fill="url(#coreGlow)" />
    <circle cx="200" cy="144" r="6" fill="#91CAFF" />
    <circle cx="200" cy="144" r="2.5" fill="#FFFFFF" />
  </g>
</svg>
`;

// 2. CONCEPT B: THE "Z · DOT" MONOGRAM (The Dynamic Island Monogram)
// Top pill = notch, bottom pill = expanded island, joined by a 45° kinetic slash.
const svgMonogramZ = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="zGradTop" x1="80" y1="100" x2="320" y2="100" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#91CAFF" />
      <stop offset="50%" stop-color="#38BDF8" />
      <stop offset="100%" stop-color="#1E3A8A" />
    </linearGradient>
    <linearGradient id="zBodyGrad" x1="100" y1="90" x2="300" y2="310" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#161F2E" />
      <stop offset="50%" stop-color="#0E1420" />
      <stop offset="100%" stop-color="#070A0F" />
    </linearGradient>
    <linearGradient id="zNeonRim" x1="80" y1="80" x2="320" y2="320" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.6" />
      <stop offset="25%" stop-color="#91CAFF" />
      <stop offset="70%" stop-color="rgba(145, 202, 255, 0.2)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.4" />
    </linearGradient>
    <radialGradient id="zDotLight" cx="50%" cy="50%" r="50%">
      <stop offset="0%" stop-color="#FFFFFF" />
      <stop offset="30%" stop-color="#91CAFF" />
      <stop offset="70%" stop-color="#0284C7" stop-opacity="0.4" />
      <stop offset="100%" stop-color="#0369A1" stop-opacity="0" />
    </radialGradient>
    <filter id="zShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="24" stdDeviation="28" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="4" stdDeviation="12" flood-color="#0C1422" flood-opacity="0.7" />
    </filter>
    <filter id="zGlow" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Subtly inclined dynamic background speed track -->
  <line x1="80" y1="320" x2="320" y2="80" stroke="rgba(145, 202, 255, 0.08)" stroke-width="2" stroke-dasharray="6 10" />

  <g filter="url(#zShadow)">
    <!-- THE MONOGRAM 'Z' (Carved from Dynamic Island Pills) -->
    <!-- Top Horizontal Notch Pill -->
    <rect x="90" y="92" width="220" height="52" rx="26" fill="url(#zBodyGrad)" stroke="url(#zNeonRim)" stroke-width="2.5" />

    <!-- Diagonal Kinetic Bridge (Connecting Top to Bottom to form 'Z') -->
    <path d="
      M 256 128
      L 144 272
      L 186 272
      L 298 128
      Z
    " fill="url(#zBodyGrad)" stroke="url(#zNeonRim)" stroke-width="2.5" stroke-linejoin="round" />

    <!-- Bottom Horizontal Expanded Island Pill -->
    <rect x="90" y="256" width="220" height="52" rx="26" fill="url(#zBodyGrad)" stroke="url(#zNeonRim)" stroke-width="2.5" />
  </g>

  <!-- Cutout Accents / Inner Glow -->
  <!-- Top Pill Left Anchor -->
  <circle cx="116" cy="118" r="8" fill="#131C2A" stroke="rgba(145, 202, 255, 0.2)" stroke-width="1.5" />
  
  <!-- Bottom Pill Right Anchor -->
  <circle cx="284" cy="282" r="8" fill="#131C2A" stroke="rgba(145, 202, 255, 0.2)" stroke-width="1.5" />

  <!-- The Optical Center: THE ZENITH DOT (Suspended in the diagonal crossover) -->
  <g filter="url(#zGlow)">
    <circle cx="200" cy="200" r="22" fill="url(#zDotLight)" />
    <circle cx="200" cy="200" r="10" fill="#91CAFF" />
    <circle cx="200" cy="200" r="4.5" fill="#FFFFFF" />
  </g>
</svg>
`;

// 3. CONCEPT C: THE TRUE ZENITH APEX (Astronomical Apex & Kinetic Pill)
// Minimalist, pure Linear/Raycast tier. A chamfered Dynamic Island with an apex rising at Zenith.
const svgApexPure = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="apexGrad" x1="60" y1="120" x2="340" y2="280" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#1A2434" />
      <stop offset="45%" stop-color="#0E1522" />
      <stop offset="100%" stop-color="#06090F" />
    </linearGradient>
    <linearGradient id="apexEdge" x1="60" y1="80" x2="340" y2="300" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.7" />
      <stop offset="25%" stop-color="#91CAFF" />
      <stop offset="60%" stop-color="rgba(145, 202, 255, 0.15)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.3" />
    </linearGradient>
    <linearGradient id="beamGrad" x1="200" y1="60" x2="200" y2="200" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#91CAFF" stop-opacity="0.8" />
      <stop offset="100%" stop-color="#0284C7" stop-opacity="0" />
    </linearGradient>
    <filter id="aShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="26" stdDeviation="30" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="12" flood-color="#0C1422" flood-opacity="0.7" />
    </filter>
    <filter id="aGlow" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="9" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Astronomical Zenith Meridian Ray -->
  <line x1="200" y1="50" x2="200" y2="350" stroke="rgba(145, 202, 255, 0.1)" stroke-width="1.5" stroke-dasharray="4 8" />
  
  <!-- Outer Horizon Ring -->
  <circle cx="200" cy="200" r="145" stroke="rgba(145, 202, 255, 0.08)" stroke-width="1.5" />

  <!-- The Apex Dynamic Island -->
  <g filter="url(#aShadow)">
    <!-- Pill with Apex Rising upward towards the Zenith point -->
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
    " fill="url(#apexGrad)" stroke="url(#apexEdge)" stroke-width="2.8" />
  </g>

  <!-- Zenith Apex Projection Beam -->
  <path d="M 188 126 L 200 90 L 212 126 Z" fill="url(#beamGrad)" />

  <!-- The Singular Floating Polaris Zenith Dot -->
  <g filter="url(#aGlow)">
    <circle cx="200" cy="90" r="16" fill="rgba(145, 202, 255, 0.3)" />
    <circle cx="200" cy="90" r="8" fill="#91CAFF" />
    <circle cx="200" cy="90" r="3" fill="#FFFFFF" />
  </g>

  <!-- Pill Internal Audio / Status Slot -->
  <rect x="120" y="194" width="160" height="12" rx="6" fill="#0A0E16" stroke="rgba(145, 202, 255, 0.15)" stroke-width="1" />
  <circle cx="132" cy="200" r="3.5" fill="#38BDF8" />
  <circle cx="268" cy="200" r="3.5" fill="#91CAFF" />
</svg>
`;

// 4. CONCEPT D: THE MINIMALIST NEGATIVE-SPACE BEZEL CAT (Clever Optical Illusion)
// A sleek Dynamic Island Pill where the top cutout in negative space forms subtle cat ears!
const svgNegativeSpaceCat = `
<svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
  <defs>
    <linearGradient id="negPillGrad" x1="60" y1="130" x2="340" y2="270" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#1B2536" />
      <stop offset="50%" stop-color="#0E1420" />
      <stop offset="100%" stop-color="#06090E" />
    </linearGradient>
    <linearGradient id="negRimGrad" x1="60" y1="100" x2="340" y2="300" gradientUnits="userSpaceOnUse">
      <stop offset="0%" stop-color="#FFFFFF" stop-opacity="0.6" />
      <stop offset="30%" stop-color="#91CAFF" />
      <stop offset="70%" stop-color="rgba(145, 202, 255, 0.2)" />
      <stop offset="100%" stop-color="#38BDF8" stop-opacity="0.4" />
    </linearGradient>
    <filter id="nShadow" x="-20%" y="-20%" width="140%" height="140%">
      <feDropShadow dx="0" dy="26" stdDeviation="30" flood-color="#000000" flood-opacity="0.95" />
      <feDropShadow dx="0" dy="6" stdDeviation="12" flood-color="#0C1422" flood-opacity="0.7" />
    </filter>
    <filter id="nGlow" x="-30%" y="-30%" width="160%" height="160%">
      <feGaussianBlur stdDeviation="8" result="blur" />
      <feMerge>
        <feMergeNode in="blur" />
        <feMergeNode in="SourceGraphic" />
      </feMerge>
    </filter>
  </defs>

  <!-- Top Display Bezel Reference -->
  <line x1="50" y1="120" x2="350" y2="120" stroke="rgba(255, 255, 255, 0.1)" stroke-width="2" stroke-linecap="round" />

  <g filter="url(#nShadow)">
    <!-- Dynamic Island Pill Docked at Top Bezel with Subtle Cat Ears carved into silhouette -->
    <path d="
      M 80 140
      C 80 140, 120 140, 136 140
      L 154 106
      C 158 98, 166 104, 172 120
      C 180 134, 190 138, 200 138
      C 210 138, 220 134, 228 120
      C 234 104, 242 98, 246 106
      L 264 140
      C 280 140, 320 140, 320 140
      C 320 190, 320 220, 320 230
      C 320 270, 270 270, 200 270
      C 130 270, 80 270, 80 230
      C 80 220, 80 190, 80 140 Z
    " fill="url(#negPillGrad)" stroke="url(#negRimGrad)" stroke-width="2.8" stroke-linejoin="round" />
  </g>

  <!-- Glowing Cat Eyes hidden subtly in the dark pill surface (like Apple Sleep LED) -->
  <g filter="url(#nGlow)">
    <!-- Left minimal eye -->
    <rect x="145" y="190" width="22" height="6" rx="3" fill="#91CAFF" />
    <!-- Right minimal eye -->
    <rect x="233" y="190" width="22" height="6" rx="3" fill="#91CAFF" />
    
    <!-- Central Forehead Zenith Dot -->
    <circle cx="200" cy="168" r="8" fill="#91CAFF" />
    <circle cx="200" cy="168" r="3" fill="#FFFFFF" />
  </g>

  <!-- Audio Waveform Sub-bar at the bottom -->
  <g opacity="0.6">
    <line x1="165" y1="230" x2="165" y2="242" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round" />
    <line x1="175" y1="226" x2="175" y2="246" stroke="#91CAFF" stroke-width="2.5" stroke-linecap="round" />
    <line x1="185" y1="222" x2="185" y2="250" stroke="#91CAFF" stroke-width="2.5" stroke-linecap="round" />
    <line x1="195" y1="220" x2="195" y2="252" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" />
    <line x1="205" y1="220" x2="205" y2="252" stroke="#FFFFFF" stroke-width="2.5" stroke-linecap="round" />
    <line x1="215" y1="222" x2="215" y2="250" stroke="#91CAFF" stroke-width="2.5" stroke-linecap="round" />
    <line x1="225" y1="226" x2="225" y2="246" stroke="#91CAFF" stroke-width="2.5" stroke-linecap="round" />
    <line x1="235" y1="230" x2="235" y2="242" stroke="#38BDF8" stroke-width="2.5" stroke-linecap="round" />
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

// 5. COMPARISON SHOWCASE SHEET (2400x1450)
const htmlMasterShowcase = `
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
      <div class="brand-title">ZENITH DOT · REFINED LOGO SUITE</div>
      <div class="brand-sub">Native Windows 11 C++20 Utility // Elite Geometric Brand Identity</div>
    </div>
    <div class="meta-tag">100% ANTI-SLOP · ZERO DOODLE · MATHEMATICAL CAD PRECISION</div>
  </div>

  <div class="grid">
    <!-- Card 1: Geometric Stealth Cat -->
    <div class="card">
      <div class="card-badge">Option A // Stealth Emblem</div>
      <div class="stage">
        ${svgCatGeometric.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Stealth Panther</div>
        <div class="sub">Origami / Aerospace Faceted Mark</div>
        <div class="desc">Bukan kartun, melainkan emblem geometris tajam (ala Lamborghini/Razer). Garis presisi 45°, visor laser cyan, dan Zenith Dot di dahi.</div>
      </div>
    </div>

    <!-- Card 2: Monogram Z -->
    <div class="card">
      <div class="card-badge">Option B // Pure Monogram</div>
      <div class="stage">
        ${svgMonogramZ.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Dynamic 'Z' Monogram</div>
        <div class="sub">Dual Island Kinetic Letterform</div>
        <div class="desc">Huruf 'Z' arsitektural yang dibentuk dari dua Dynamic Island pill (notch & expanded) yang terhubung kinetic slash dengan Dot di pusat optik.</div>
      </div>
    </div>

    <!-- Card 3: Zenith Apex -->
    <div class="card">
      <div class="card-badge">Option C // Horizon Apex</div>
      <div class="stage">
        ${svgApexPure.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Zenith Apex</div>
        <div class="sub">Astronomical Meridian & Lens</div>
        <div class="desc">Filosofi murni arti kata 'Zenith' (titik tertinggi langit). Dynamic pill chamfered yang memuncak ke atas mengarahkan cahaya ke bintang Polaris Dot.</div>
      </div>
    </div>

    <!-- Card 4: Negative Space Bezel Cat -->
    <div class="card">
      <div class="card-badge">Option D // Optical Illusion</div>
      <div class="stage">
        ${svgNegativeSpaceCat.replace('width="400" height="400"', 'width="280" height="280"')}
      </div>
      <div class="footer">
        <div class="title">The Bezel Cat (Negative Space)</div>
        <div class="sub">Smart Dynamic Island Illusion</div>
        <div class="desc">Dari jauh tampak seperti Dynamic Island pill Windows 11 biasa. Namun lekukan atasnya membentuk siluet telinga kucing secara cerdas dan elegan.</div>
      </div>
    </div>
  </div>
</body>
</html>
`;

async function renderRefinedSuite() {
  const browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });

  const tasks = [
    {
      name: 'zenith-v2-stealth-panther.png',
      html: buildCardHtml('The Stealth Panther', 'Aerospace Geometric Emblem', 'Concept A // Stealth', svgCatGeometric),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'zenith-v2-monogram-z.png',
      html: buildCardHtml('The Dynamic Monogram', 'Dual Island Kinetic "Z"', 'Concept B // Monogram', svgMonogramZ),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'zenith-v2-zenith-apex.png',
      html: buildCardHtml('The Zenith Apex', 'Astronomical Meridian & Core Dot', 'Concept C // Zenith Apex', svgApexPure),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'zenith-v2-negative-cat.png',
      html: buildCardHtml('The Bezel Cat', 'Negative Space Dynamic Island', 'Concept D // Optical Illusion', svgNegativeSpaceCat),
      w: 1024, h: 1024, s: 2
    },
    {
      name: 'zenith-v2-master-showcase.png',
      html: htmlMasterShowcase,
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
  console.log('ALL REFINED V2 LOGOS RENDERED SUCCESSFULLY!');
}

renderRefinedSuite().catch(err => {
  console.error(err);
  process.exit(1);
});
