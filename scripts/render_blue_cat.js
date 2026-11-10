const { chromium } = require('playwright');
const fs = require('fs');
const path = require('path');

const OUTPUT_DIR = '/home/alifihsan/Zenith-Dot/assets/logos';
if (!fs.existsSync(OUTPUT_DIR)) {
  fs.mkdirSync(OUTPUT_DIR, { recursive: true });
}

// 1. PRIMARY APP ICON: BLUE & BLACK OBSIDIAN CAT (2048x2048 Retina)
const htmlAppIcon = `
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
  .notch-guide {
    position: absolute;
    top: 48px;
    width: 280px; height: 18px;
    border-bottom-left-radius: 12px;
    border-bottom-right-radius: 12px;
    background: rgba(0, 0, 0, 0.4);
    border: 1px solid rgba(255, 255, 255, 0.06);
    border-top: none;
    display: flex; align-items: center; justify-content: center;
  }
  .notch-cam {
    width: 6px; height: 6px; border-radius: 50%;
    background: #1e293b;
    border: 1px solid rgba(145, 202, 255, 0.2);
  }
  .label-group {
    margin-top: 36px;
    text-align: center;
  }
  .title {
    font-size: 36px;
    font-weight: 800;
    letter-spacing: 5px;
    color: #F5F6F8;
    text-transform: uppercase;
  }
  .subtitle {
    margin-top: 8px;
    font-size: 15px;
    font-weight: 600;
    letter-spacing: 3.5px;
    color: #91CAFF;
    text-transform: uppercase;
  }
  .tech-badge {
    margin-top: 6px;
    font-family: "JetBrains Mono", monospace;
    font-size: 12px;
    letter-spacing: 2px;
    color: #64748B;
  }
</style>
</head>
<body>
  <div class="bg-radial"></div>
  <div class="squircle">
    <div class="notch-guide"><div class="notch-cam"></div></div>

    <!-- Scalable Cat SVG (Blue & Black Edition) -->
    <svg width="400" height="400" viewBox="0 0 400 400" fill="none" xmlns="http://www.w3.org/2000/svg">
      <defs>
        <!-- Blue-tinted Liquid Obsidian Body -->
        <linearGradient id="catBodyBlue" x1="80" y1="60" x2="320" y2="340" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="#1E2838" />
          <stop offset="35%" stop-color="#101826" />
          <stop offset="70%" stop-color="#090E17" />
          <stop offset="100%" stop-color="#04060A" />
        </linearGradient>

        <!-- Zenith Blue Rim Lighting -->
        <linearGradient id="catRimBlue" x1="60" y1="40" x2="340" y2="360" gradientUnits="userSpaceOnUse">
          <stop offset="0%" stop-color="rgba(255, 255, 255, 0.6)" />
          <stop offset="25%" stop-color="#91CAFF" />
          <stop offset="55%" stop-color="rgba(56, 189, 248, 0.4)" />
          <stop offset="85%" stop-color="rgba(12, 20, 34, 0.2)" />
          <stop offset="100%" stop-color="rgba(145, 202, 255, 0.1)" />
        </linearGradient>

        <!-- Eye Luminescence Gradient -->
        <linearGradient id="catEyeBlue" x1="0" y1="0" x2="0" y2="1">
          <stop offset="0%" stop-color="#E0F2FE" />
          <stop offset="50%" stop-color="#91CAFF" />
          <stop offset="100%" stop-color="#0284C7" />
        </linearGradient>

        <!-- Forehead Zenith Dot Glow -->
        <radialGradient id="zenithDotGlow" cx="50%" cy="50%" r="50%">
          <stop offset="0%" stop-color="#C2E0FF" stop-opacity="1" />
          <stop offset="40%" stop-color="#91CAFF" stop-opacity="0.8" />
          <stop offset="80%" stop-color="#0284C7" stop-opacity="0.2" />
          <stop offset="100%" stop-color="#0369A1" stop-opacity="0" />
        </radialGradient>

        <!-- Drop Shadow -->
        <filter id="catShadow" x="-25%" y="-25%" width="150%" height="150%">
          <feDropShadow dx="0" dy="20" stdDeviation="24" flood-color="#000000" flood-opacity="0.9" />
          <feDropShadow dx="0" dy="8" stdDeviation="10" flood-color="#0C1422" flood-opacity="0.6" />
        </filter>

        <!-- Blue Glow Filter -->
        <filter id="blueNeonGlow" x="-30%" y="-30%" width="160%" height="160%">
          <feGaussianBlur stdDeviation="7" result="blur" />
          <feMerge>
            <feMergeNode in="blur" />
            <feMergeNode in="SourceGraphic" />
          </feMerge>
        </filter>
      </defs>

      <!-- Ambient Orbital Ring around the Cat Head -->
      <circle cx="200" cy="200" r="160" stroke="rgba(145, 202, 255, 0.08)" stroke-width="1.5" stroke-dasharray="6 12" />

      <!-- The Cat Body (Main Ferrofluid Form) -->
      <g filter="url(#catShadow)">
        <path d="
          M 95 200
          C 95 135, 140 115, 134 76
          C 131 58, 155 64, 172 94
          C 184 105, 216 105, 228 94
          C 245 64, 269 58, 266 76
          C 260 115, 305 135, 305 200
          C 305 265, 260 295, 200 295
          C 140 295, 95 265, 95 200 Z
        " fill="url(#catBodyBlue)" stroke="url(#catRimBlue)" stroke-width="2.8" />

        <!-- Liquid Specular Sheen (Top Gloss Highlight) -->
        <path d="
          M 139 84 
          C 149 72, 161 82, 170 96
          C 185 110, 215 110, 230 96
          C 239 82, 251 72, 261 84
        " stroke="rgba(255,255,255,0.7)" stroke-width="2.2" stroke-linecap="round" fill="none" />
      </g>

      <!-- Liquid Cyber Eyes (Sleek Slits, Watchful & Calm) -->
      <g filter="url(#blueNeonGlow)">
        <!-- Left Eye -->
        <path d="M 148 194 Q 168 180 180 194 Q 168 204 148 194 Z" fill="url(#catEyeBlue)" />
        <ellipse cx="164" cy="193" rx="2.5" ry="6" fill="#06121E" />

        <!-- Right Eye -->
        <path d="M 220 194 Q 232 180 252 194 Q 232 204 220 194 Z" fill="url(#catEyeBlue)" />
        <ellipse cx="236" cy="193" rx="2.5" ry="6" fill="#06121E" />
      </g>

      <!-- Signature Zenith Dot (Luminous Forehead Core) -->
      <circle cx="200" cy="146" r="16" fill="url(#zenithDotGlow)" />
      <circle cx="200" cy="146" r="7" fill="#91CAFF" filter="url(#blueNeonGlow)" />
      <circle cx="200" cy="146" r="3" fill="#FFFFFF" />

      <!-- Minimalist Whisker / Audio Waveform Bars (Interactive Media State) -->
      <g stroke-linecap="round">
        <!-- Left Audio Whiskers -->
        <line x1="78" y1="190" x2="108" y2="190" stroke="#91CAFF" stroke-width="2.5" opacity="0.8" />
        <line x1="70" y1="204" x2="104" y2="207" stroke="#38BDF8" stroke-width="2" opacity="0.5" />
        <line x1="82" y1="218" x2="106" y2="222" stroke="#0284C7" stroke-width="1.8" opacity="0.3" />

        <!-- Right Audio Whiskers -->
        <line x1="322" y1="190" x2="292" y2="190" stroke="#91CAFF" stroke-width="2.5" opacity="0.8" />
        <line x1="330" y1="204" x2="296" y2="207" stroke="#38BDF8" stroke-width="2" opacity="0.5" />
        <line x1="318" y1="218" x2="294" y2="222" stroke="#0284C7" stroke-width="1.8" opacity="0.3" />
      </g>

      <!-- Nose Dot & Subtle Chin -->
      <circle cx="200" cy="225" r="2.8" fill="rgba(145, 202, 255, 0.3)" />
      <path d="M 194 233 Q 200 237 206 233" stroke="rgba(145, 202, 255, 0.2)" stroke-width="1.5" stroke-linecap="round" fill="none" />
    </svg>

    <div class="label-group">
      <div class="title">Zenith Dot</div>
      <div class="subtitle">The Obsidian Familiar</div>
      <div class="tech-badge">C++20 · NATIVE DYNAMIC ISLAND · DIRECT2D</div>
    </div>
  </div>
</body>
</html>
`;

// 2. HERO SHOWCASE SHEET: PRESENTING THE BLUE CAT IN CONTEXT (3600x2000)
const htmlHeroShowcase = `
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<style>
  * { box-sizing: border-box; margin: 0; padding: 0; }
  body {
    width: 2400px; height: 1400px;
    background: #06080D;
    font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Inter", sans-serif;
    color: #F5F6F8;
    display: flex; flex-direction: column;
    padding: 70px 90px;
    overflow: hidden;
    position: relative;
  }
  .bg-ambient {
    position: absolute;
    top: 0; left: 0; width: 100%; height: 100%;
    background: 
      radial-gradient(circle at 20% 30%, rgba(145, 202, 255, 0.08) 0%, transparent 50%),
      radial-gradient(circle at 80% 70%, rgba(12, 20, 34, 0.6) 0%, transparent 60%);
    pointer-events: none;
  }
  .header {
    display: flex; justify-content: space-between; align-items: flex-end;
    border-bottom: 1px solid rgba(145, 202, 255, 0.12);
    padding-bottom: 28px;
    margin-bottom: 50px;
  }
  .brand-title {
    font-size: 48px; font-weight: 800; letter-spacing: 6px;
    background: linear-gradient(180deg, #FFFFFF 0%, #91CAFF 100%);
    -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  }
  .brand-sub {
    font-size: 19px; font-weight: 500; color: #94A3B8; letter-spacing: 3px; margin-top: 6px;
  }
  .color-chips {
    display: flex; gap: 14px; align-items: center;
  }
  .chip {
    display: flex; align-items: center; gap: 10px;
    background: #0E1420;
    border: 1px solid rgba(145, 202, 255, 0.2);
    padding: 8px 16px; border-radius: 999px;
    font-family: "JetBrains Mono", monospace; font-size: 13px; color: #CBD5E1;
  }
  .chip-dot { width: 12px; height: 12px; border-radius: 50%; }

  .content-grid {
    display: grid;
    grid-template-columns: 460px 1fr;
    gap: 50px;
    flex: 1;
  }

  /* Left Hero Card */
  .hero-card {
    background: linear-gradient(160deg, #101622 0%, #080C14 100%);
    border-radius: 40px;
    border: 1px solid rgba(145, 202, 255, 0.25);
    box-shadow: 0 30px 80px rgba(0,0,0,0.8), 0 0 50px rgba(145,202,255,0.06);
    display: flex; flex-direction: column; align-items: center; justify-content: center;
    padding: 40px;
    position: relative;
  }
  .hero-badge {
    position: absolute; top: 28px;
    padding: 6px 16px; background: rgba(145, 202, 255, 0.12);
    border: 1px solid rgba(145, 202, 255, 0.3); border-radius: 999px;
    font-size: 12px; font-weight: 700; color: #91CAFF; letter-spacing: 2px;
  }

  /* Right Interactive State Cards */
  .states-container {
    display: flex; flex-direction: column; gap: 28px; justify-content: center;
  }
  .state-card {
    background: #0B0F17;
    border-radius: 28px;
    border: 1px solid rgba(255, 255, 255, 0.08);
    padding: 28px 36px;
    display: flex; align-items: center; gap: 36px;
    box-shadow: 0 15px 40px rgba(0,0,0,0.5);
  }
  .state-info { width: 340px; }
  .state-title { font-size: 20px; font-weight: 700; color: #FFFFFF; letter-spacing: 1px; }
  .state-desc { font-size: 14px; color: #8A99AD; margin-top: 6px; line-height: 1.5; }
  .state-demo { flex: 1; display: flex; align-items: center; justify-content: center; }

  /* Dynamic Island Pills Mockups */
  .bezel-screen {
    width: 100%; height: 96px;
    background: #040608;
    border-radius: 18px;
    border-top: 3px solid #1E293B;
    position: relative;
    display: flex; justify-content: center; align-items: flex-start;
  }

  /* 1. Idle Pill */
  .island-idle {
    margin-top: 0;
    width: 130px; height: 34px;
    background: #000;
    border-bottom-left-radius: 17px;
    border-bottom-right-radius: 17px;
    border: 1px solid rgba(145, 202, 255, 0.3);
    border-top: none;
    display: flex; align-items: center; justify-content: space-between;
    padding: 0 16px;
  }
  .cat-idle-face { display: flex; align-items: center; gap: 6px; }
  .cat-ear { width: 0; height: 0; border-left: 4px solid transparent; border-right: 4px solid transparent; border-bottom: 6px solid #91CAFF; }
  .cat-eye-dot { width: 4px; height: 4px; border-radius: 50%; background: #91CAFF; box-shadow: 0 0 6px #91CAFF; }

  /* 2. Media Playing Pill */
  .island-music {
    margin-top: 0;
    width: 290px; height: 40px;
    background: #090E17;
    border-bottom-left-radius: 20px;
    border-bottom-right-radius: 20px;
    border: 1px solid rgba(145, 202, 255, 0.35);
    border-top: none;
    display: flex; align-items: center; justify-content: space-between;
    padding: 0 20px;
    box-shadow: 0 8px 24px rgba(145, 202, 255, 0.15);
  }
  .track-text { font-size: 13px; font-weight: 600; color: #F5F6F8; }
  .wave-bars { display: flex; align-items: center; gap: 4px; }
  .w-bar { width: 3px; background: #91CAFF; border-radius: 2px; }

  /* 3. Drop Zone Expanded */
  .island-drop {
    margin-top: 0;
    width: 320px; height: 48px;
    background: #0D1624;
    border-bottom-left-radius: 24px;
    border-bottom-right-radius: 24px;
    border: 1.5px dashed #91CAFF;
    border-top: none;
    display: flex; align-items: center; justify-content: center; gap: 14px;
    box-shadow: 0 10px 30px rgba(145, 202, 255, 0.25);
  }
  .drop-text { font-size: 13px; font-weight: 700; color: #91CAFF; letter-spacing: 1.5px; text-transform: uppercase; }
</style>
</head>
<body>
  <div class="bg-ambient"></div>
  
  <div class="header">
    <div>
      <div class="brand-title">ZENITH DOT · OBSIDIAN FAMILIAR</div>
      <div class="brand-sub">Native Windows 11 Blue & Black Palette // Desktop Companion Persona</div>
    </div>
    <div class="color-chips">
      <div class="chip"><div class="chip-dot" style="background:#91CAFF; box-shadow:0 0 8px #91CAFF"></div>#91CAFF Accent</div>
      <div class="chip"><div class="chip-dot" style="background:#0C1422"></div>#0C1422 Tint</div>
      <div class="chip"><div class="chip-dot" style="background:#06080D"></div>#06080D Obsidian</div>
      <div class="chip"><div class="chip-dot" style="background:#F5F6F8"></div>#F5F6F8 Ink</div>
    </div>
  </div>

  <div class="content-grid">
    <!-- Left: Large App Icon -->
    <div class="hero-card">
      <div class="hero-badge">OFFICIAL ICON // 2048px</div>
      
      <svg width="300" height="300" viewBox="0 0 400 400" fill="none">
        <defs>
          <linearGradient id="hBody" x1="80" y1="60" x2="320" y2="340" gradientUnits="userSpaceOnUse">
            <stop offset="0%" stop-color="#1E2838" />
            <stop offset="35%" stop-color="#101826" />
            <stop offset="70%" stop-color="#090E17" />
            <stop offset="100%" stop-color="#04060A" />
          </linearGradient>
          <linearGradient id="hRim" x1="60" y1="40" x2="340" y2="360" gradientUnits="userSpaceOnUse">
            <stop offset="0%" stop-color="rgba(255, 255, 255, 0.6)" />
            <stop offset="30%" stop-color="#91CAFF" />
            <stop offset="70%" stop-color="rgba(12, 20, 34, 0.2)" />
            <stop offset="100%" stop-color="rgba(145, 202, 255, 0.1)" />
          </linearGradient>
          <filter id="hGlow">
            <feGaussianBlur stdDeviation="6" result="blur" />
            <feMerge><feMergeNode in="blur" /><feMergeNode in="SourceGraphic" /></feMerge>
          </filter>
        </defs>
        <path d="M 95 200 C 95 135, 140 115, 134 76 C 131 58, 155 64, 172 94 C 184 105, 216 105, 228 94 C 245 64, 269 58, 266 76 C 260 115, 305 135, 305 200 C 305 265, 260 295, 200 295 C 140 295, 95 265, 95 200 Z" fill="url(#hBody)" stroke="url(#hRim)" stroke-width="3" />
        <!-- Eyes -->
        <g filter="url(#hGlow)">
          <path d="M 148 194 Q 168 180 180 194 Q 168 204 148 194 Z" fill="#91CAFF" />
          <ellipse cx="164" cy="193" rx="2.5" ry="6" fill="#06121E" />
          <path d="M 220 194 Q 232 180 252 194 Q 232 204 220 194 Z" fill="#91CAFF" />
          <ellipse cx="236" cy="193" rx="2.5" ry="6" fill="#06121E" />
          <circle cx="200" cy="146" r="8" fill="#91CAFF" />
          <circle cx="200" cy="146" r="3" fill="#FFFFFF" />
        </g>
        <line x1="78" y1="190" x2="108" y2="190" stroke="#91CAFF" stroke-width="2.5" />
        <line x1="322" y1="190" x2="292" y2="190" stroke="#91CAFF" stroke-width="2.5" />
      </svg>

      <div style="margin-top:24px; text-align:center;">
        <div style="font-size:24px; font-weight:800; color:#FFF; letter-spacing:3px;">ZENITH DOT</div>
        <div style="font-size:13px; color:#91CAFF; letter-spacing:2px; margin-top:4px;">BLUE & OBSIDIAN EDITION</div>
      </div>
    </div>

    <!-- Right: Three Dynamic Island States in Windows 11 Notch -->
    <div class="states-container">
      <!-- State 1: Idle -->
      <div class="state-card">
        <div class="state-info">
          <div class="state-title">1. Top Bezel Idle (Sleeping Cat)</div>
          <div class="state-desc">Saat tidak aktif, notch mengecil menjadi pill minimalis 130px dengan telinga dan mata dot biru yang tersembunyi tenang di bezel atas.</div>
        </div>
        <div class="state-demo">
          <div class="bezel-screen">
            <div class="island-idle">
              <div class="cat-ear" style="transform:rotate(-15deg);"></div>
              <div class="cat-idle-face">
                <div class="cat-eye-dot"></div>
                <div style="width:18px; height:1px; background:rgba(145,202,255,0.4);"></div>
                <div class="cat-eye-dot"></div>
              </div>
              <div class="cat-ear" style="transform:rotate(15deg);"></div>
            </div>
          </div>
        </div>
      </div>

      <!-- State 2: Media Playing -->
      <div class="state-card">
        <div class="state-info">
          <div class="state-title">2. Media Playback (Stretching Pill)</div>
          <div class="state-desc">Saat memutar Spotify/media, pill meregang lentur (*spring physics*), menampilkan judul lagu dan visualizer gelombang audio kumis biru.</div>
        </div>
        <div class="state-demo">
          <div class="bezel-screen">
            <div class="island-music">
              <div style="display:flex; align-items:center; gap:10px;">
                <div style="width:8px; height:8px; border-radius:50%; background:#91CAFF; box-shadow:0 0 6px #91CAFF;"></div>
                <div class="track-text">Starboy · The Weeknd</div>
              </div>
              <div class="wave-bars">
                <div class="w-bar" style="height:12px;"></div>
                <div class="w-bar" style="height:20px;"></div>
                <div class="w-bar" style="height:16px;"></div>
                <div class="w-bar" style="height:24px;"></div>
                <div class="w-bar" style="height:10px;"></div>
              </div>
            </div>
          </div>
        </div>
      </div>

      <!-- State 3: Drop Zone Active -->
      <div class="state-card">
        <div class="state-info">
          <div class="state-title">3. OLE Drop Zone (Catching Payload)</div>
          <div class="state-desc">Saat file di-drag ke atas layar, notch membesar dengan border neon biru dan membuka ruang drop buffer instan (*Show in folder*).</div>
        </div>
        <div class="state-demo">
          <div class="bezel-screen">
            <div class="island-drop">
              <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#91CAFF" stroke-width="2.5"><path d="M12 3v12m0 0l-4-4m4 4l4-4M4 17v2a2 2 0 002 2h12a2 2 0 002-2v-2" stroke-linecap="round"/></svg>
              <div class="drop-text">Drop Files To Cat Buffer</div>
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</body>
</html>
`;

async function renderBlueSuite() {
  const browser = await chromium.launch({ headless: true, args: ['--no-sandbox'] });

  // 1. App Icon
  const page1 = await browser.newPage({ viewport: { width: 1024, height: 1024, deviceScaleFactor: 2 } });
  await page1.setContent(htmlAppIcon, { waitUntil: 'load' });
  const iconPath = path.join(OUTPUT_DIR, 'zenith-cat-blue-appicon.png');
  await page1.screenshot({ path: iconPath });
  console.log('Rendered:', iconPath);
  await page1.close();

  // 2. Hero Showcase
  const page2 = await browser.newPage({ viewport: { width: 2400, height: 1400, deviceScaleFactor: 1.5 } });
  await page2.setContent(htmlHeroShowcase, { waitUntil: 'load' });
  const heroPath = path.join(OUTPUT_DIR, 'zenith-cat-blue-showcase.png');
  await page2.screenshot({ path: heroPath });
  console.log('Rendered:', heroPath);
  await page2.close();

  await browser.close();
  console.log('BLUE OBSIDIAN SUITE RENDERED!');
}

renderBlueSuite().catch(err => {
  console.error(err);
  process.exit(1);
});
