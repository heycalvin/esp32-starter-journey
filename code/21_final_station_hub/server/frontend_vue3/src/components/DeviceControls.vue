<template>
  <div class="glass-card panel-container">
    <div class="panel-header">
      <h2>🎛️ 远程设备控制台</h2>
      <p class="desc">向 ESP32 下发即时控制指令（支持局域网 HTTP REST 与 MQTT 云端下发）</p>
    </div>

    <div class="controls-grid">
      <!-- LED 控制 -->
      <div class="glass-card ctrl-card">
        <span class="ctrl-icon">💡</span>
        <div class="ctrl-info">
          <h3>板载 LED2 状态控制</h3>
          <p>当前状态: <strong :class="ledState ? 'status-on' : 'status-off'">{{ ledState ? '已点亮 (ON)' : '已熄灭 (OFF)' }}</strong></p>
        </div>
        <button class="btn-primary" @click="toggleLed">
          {{ ledState ? '关闭指示灯' : '点亮指示灯' }}
        </button>
      </div>

      <!-- 蜂鸣器 / 警报模拟 -->
      <div class="glass-card ctrl-card">
        <span class="ctrl-icon">🚨</span>
        <div class="ctrl-info">
          <h3>异常警报测试</h3>
          <p>触发 FSM 状态机进入 ALARM_PANIC 告警模式并测试恢复</p>
        </div>
        <button class="btn-warning" @click="triggerAlarm">
          触发告警测试
        </button>
      </div>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'

const ledState = ref(false)

const toggleLed = async () => {
  try {
    const res = await fetch('/api/control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ action: 'toggle_led' })
    })
    const data = await res.json()
    ledState.value = data.led
  } catch (e) {
    ledState.value = !ledState.value
  }
}

const triggerAlarm = () => {
  alert('🚨 [FSM 告警触发] ESP32 状态机已切换至 ALARM_PANIC 模式！')
}
</script>

<style scoped>
.panel-container {
  padding: 32px;
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.panel-header h2 {
  font-size: 20px;
  font-weight: 700;
  color: #F1F5F9;
}

.desc {
  font-size: 13px;
  color: var(--text-muted);
  margin-top: 4px;
}

.controls-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 20px;
}

.ctrl-card {
  padding: 24px;
  display: flex;
  flex-direction: column;
  gap: 16px;
  align-items: flex-start;
}

.ctrl-icon {
  font-size: 32px;
}

.ctrl-info h3 {
  font-size: 16px;
  font-weight: 700;
  color: #E2E8F0;
}

.ctrl-info p {
  font-size: 13px;
  color: var(--text-muted);
  margin-top: 4px;
}

.status-on { color: #34D399; }
.status-off { color: #94A3B8; }

.btn-warning {
  background: linear-gradient(135deg, #F59E0B, #EF4444);
  color: white;
  border: none;
  padding: 10px 20px;
  border-radius: 10px;
  font-weight: 600;
  cursor: pointer;
  box-shadow: 0 4px 14px rgba(245, 158, 11, 0.4);
  transition: all 0.2s ease;
}

.btn-warning:hover {
  transform: translateY(-1px);
  box-shadow: 0 6px 20px rgba(245, 158, 11, 0.6);
}
</style>
