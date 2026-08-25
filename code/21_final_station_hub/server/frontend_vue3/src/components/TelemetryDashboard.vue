<template>
  <div class="dashboard-grid">
    <!-- 温度卡片 -->
    <div class="glass-card stat-card">
      <div class="card-header">
        <span class="card-icon">🌡️</span>
        <span class="card-title">室内环境气温 (NTC)</span>
      </div>
      <div class="card-value">{{ telemetry.temp.toFixed(1) }} <span class="unit">°C</span></div>
      <div class="progress-bar-bg">
        <div class="progress-bar-fill temp-bar" :style="{ width: (telemetry.temp / 50 * 100) + '%' }"></div>
      </div>
    </div>

    <!-- 湿度卡片 -->
    <div class="glass-card stat-card">
      <div class="card-header">
        <span class="card-icon">💧</span>
        <span class="card-title">相对湿度 (DHT11)</span>
      </div>
      <div class="card-value">{{ telemetry.humi.toFixed(1) }} <span class="unit">%</span></div>
      <div class="progress-bar-bg">
        <div class="progress-bar-fill humi-bar" :style="{ width: telemetry.humi + '%' }"></div>
      </div>
    </div>

    <!-- 超声波测距卡片 -->
    <div class="glass-card stat-card">
      <div class="card-header">
        <span class="card-icon">📡</span>
        <span class="card-title">雷达测距 (HC-SR04)</span>
      </div>
      <div class="card-value">{{ telemetry.dist.toFixed(1) }} <span class="unit">cm</span></div>
      <div class="progress-bar-bg">
        <div class="progress-bar-fill dist-bar" :style="{ width: Math.min(100, telemetry.dist) + '%' }"></div>
      </div>
    </div>

    <!-- 内存与算力状态 -->
    <div class="glass-card stat-card">
      <div class="card-header">
        <span class="card-icon">💻</span>
        <span class="card-title">系统可用堆内存</span>
      </div>
      <div class="card-value">{{ telemetry.free_heap_kb }} <span class="unit">KB</span></div>
      <div class="badge-row">
        <span class="badge psram-badge">PSRAM: {{ telemetry.psram_free_mb }} MB</span>
        <span class="badge online-badge">● 在线运行中</span>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref, onMounted, onUnmounted } from 'vue'

const telemetry = ref({
  temp: 26.5,
  humi: 60.2,
  dist: 22.4,
  free_heap_kb: 186,
  psram_free_mb: 1.8,
  led_state: false
})

let timer = null

const fetchStatus = async () => {
  try {
    const res = await fetch('/api/status')
    if (res.ok) {
      telemetry.value = await res.json()
    }
  } catch (e) {
    // mock fallback
  }
}

onMounted(() => {
  fetchStatus()
  timer = setInterval(fetchStatus, 1500)
})

onUnmounted(() => {
  if (timer) clearInterval(timer)
})
</script>

<style scoped>
.dashboard-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(260px, 1fr));
  gap: 20px;
}

.stat-card {
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.card-header {
  display: flex;
  align-items: center;
  gap: 8px;
}

.card-icon {
  font-size: 20px;
}

.card-title {
  font-size: 13px;
  font-weight: 600;
  color: var(--text-muted);
}

.card-value {
  font-size: 36px;
  font-weight: 800;
  color: #F8FAFC;
  font-family: 'JetBrains Mono', monospace;
}

.unit {
  font-size: 18px;
  color: var(--text-muted);
  font-weight: 500;
}

.progress-bar-bg {
  width: 100%;
  height: 8px;
  background: rgba(255, 255, 255, 0.08);
  border-radius: 4px;
  overflow: hidden;
}

.progress-bar-fill {
  height: 100%;
  border-radius: 4px;
  transition: width 0.4s ease;
}

.temp-bar { background: linear-gradient(90deg, #F97316, #EF4444); }
.humi-bar { background: linear-gradient(90deg, #06B6D4, #3B82F6); }
.dist-bar { background: linear-gradient(90deg, #10B981, #34D399); }

.badge-row {
  display: flex;
  gap: 8px;
}

.badge {
  font-size: 11px;
  font-weight: 600;
  padding: 4px 10px;
  border-radius: 20px;
}

.psram-badge {
  background: rgba(139, 92, 246, 0.15);
  color: #A78BFA;
  border: 1px solid rgba(139, 92, 246, 0.3);
}

.online-badge {
  background: rgba(16, 185, 129, 0.15);
  color: #34D399;
  border: 1px solid rgba(16, 185, 129, 0.3);
}
</style>
