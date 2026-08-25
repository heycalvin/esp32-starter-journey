<template>
  <div class="app-container">
    <!-- 顶部导航栏 -->
    <header class="navbar glass-card">
      <div class="logo-box">
        <span class="logo-icon">🚀</span>
        <div class="logo-text">
          <h1>ESP32 SMART HUB</h1>
          <span class="subtitle">桌面智能中控台 · Web 综合管理中枢 (Rust + Vue 3)</span>
        </div>
      </div>
      <div class="nav-links">
        <button 
          v-for="tab in tabs" 
          :key="tab.id" 
          :class="['nav-btn', { active: currentTab === tab.id }]"
          @click="currentTab = tab.id"
        >
          <span class="nav-icon">{{ tab.icon }}</span>
          {{ tab.name }}
        </button>
      </div>
    </header>

    <!-- 主体内容区域 -->
    <main class="main-content">
      <TelemetryDashboard v-if="currentTab === 'telemetry'" />
      <PhotoManager v-else-if="currentTab === 'photos'" />
      <NovelManager v-else-if="currentTab === 'novels'" />
      <OtaManager v-else-if="currentTab === 'ota'" />
      <DeviceControls v-else-if="currentTab === 'controls'" />
    </main>
  </div>
</template>

<script setup>
import { ref } from 'vue'
import TelemetryDashboard from './components/TelemetryDashboard.vue'
import PhotoManager from './components/PhotoManager.vue'
import NovelManager from './components/NovelManager.vue'
import OtaManager from './components/OtaManager.vue'
import DeviceControls from './components/DeviceControls.vue'

const currentTab = ref('telemetry')

const tabs = [
  { id: 'telemetry', name: '实时看板', icon: '📊' },
  { id: 'photos',    name: '相册推送', icon: '🖼️' },
  { id: 'novels',    name: '电子书下发', icon: '📖' },
  { id: 'ota',       name: '无线 OTA', icon: '🚀' },
  { id: 'controls',  name: '设备遥控', icon: '🎛️' }
]
</script>

<style scoped>
.app-container {
  max-width: 1280px;
  margin: 0 auto;
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.navbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px 24px;
}

.logo-box {
  display: flex;
  align-items: center;
  gap: 12px;
}

.logo-icon {
  font-size: 28px;
}

.logo-text h1 {
  font-size: 18px;
  font-weight: 800;
  letter-spacing: 0.5px;
  background: linear-gradient(135deg, #38BDF8, #818CF8);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}

.subtitle {
  font-size: 11px;
  color: var(--text-muted);
}

.nav-links {
  display: flex;
  gap: 8px;
}

.nav-btn {
  background: transparent;
  border: 1px solid transparent;
  color: var(--text-muted);
  padding: 8px 16px;
  border-radius: 10px;
  font-size: 14px;
  font-weight: 600;
  cursor: pointer;
  display: flex;
  align-items: center;
  gap: 6px;
  transition: all 0.2s ease;
}

.nav-btn:hover {
  color: var(--text-main);
  background: rgba(255, 255, 255, 0.05);
}

.nav-btn.active {
  color: #38BDF8;
  background: rgba(56, 189, 248, 0.12);
  border-color: rgba(56, 189, 248, 0.3);
}

.main-content {
  display: flex;
  flex-direction: column;
  gap: 24px;
}
</style>
