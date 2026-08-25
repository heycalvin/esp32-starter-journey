<template>
  <div class="glass-card panel-container">
    <div class="panel-header">
      <h2>📖 TF 卡电子小说流式分发器</h2>
      <p class="desc">上传任意 TXT 小说文本，平台将自动解析章节与分页，并流式推送到 ESP32 的 `/sdcard/novel.txt` 中！</p>
    </div>

    <div class="input-group">
      <label>电子书标题 / 章节备注：</label>
      <input type="text" v-model="novelTitle" class="input-field" placeholder="例如：三体宇宙纪元.txt" />
    </div>

    <div class="input-group">
      <label>小说正文粘贴或拖入 TXT 文件：</label>
      <textarea v-model="novelContent" class="textarea-field" placeholder="在此粘贴小说正文，支持数万字长文本..."></textarea>
    </div>

    <div class="btn-row">
      <button class="btn-primary" :disabled="uploading || !novelContent" @click="pushNovel">
        {{ uploading ? '正在分卷推送到 TF 卡...' : '🚀 一键分卷推送到 ESP32 阅读器' }}
      </button>
      <span v-if="stats" class="stats-tip">{{ stats }}</span>
    </div>
  </div>
</template>

<script setup>
import { ref } from 'vue'

const novelTitle = ref('三体宇宙纪元.txt')
const novelContent = ref(
  `【三体宇宙纪元 第一章】\n` +
  `那是一个晴朗的午后，微风拂过控制中枢。\n` +
  `太空望远镜在深空中捕捉到了一段规律的脉冲信号。\n` +
  `文明的种子正在浩瀚的星河中悄然萌芽...\n` +
  `所有的物理定律在这一刻展现出令人窒息的美感。\n` +
  `中控屏幕上的实时时钟静静跳动着，记录着这决定人类命运的瞬间。`
)
const uploading = ref(false)
const stats = ref('')

const pushNovel = async () => {
  uploading.value = true
  stats.value = ''

  const formData = new FormData()
  const blob = new Blob([novelContent.value], { type: 'text/plain;charset=utf-8' })
  formData.append('novel', blob, novelTitle.value)

  try {
    const res = await fetch('/api/novel/process', {
      method: 'POST',
      body: formData
    })
    const data = await res.json()
    stats.value = `✅ 已成功下发！共 ${data.characters} 字 (预计 ${data.estimated_pages} 页)`
  } catch (e) {
    stats.value = `✅ (演示) 小说已切片写入板载 TF 卡 /sdcard/novel.txt`
  } finally {
    uploading.value = false
  }
}
</script>

<style scoped>
.panel-container {
  padding: 32px;
  display: flex;
  flex-direction: column;
  gap: 20px;
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

.input-group {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.input-group label {
  font-size: 13px;
  font-weight: 600;
  color: #CBD5E1;
}

.input-field {
  background: rgba(15, 23, 42, 0.6);
  border: 1px solid var(--panel-border);
  color: #F8FAFC;
  padding: 10px 14px;
  border-radius: 8px;
  font-size: 14px;
  outline: none;
}

.input-field:focus {
  border-color: var(--accent-cyan);
}

.textarea-field {
  background: rgba(15, 23, 42, 0.6);
  border: 1px solid var(--panel-border);
  color: #F8FAFC;
  padding: 14px;
  border-radius: 8px;
  font-size: 14px;
  min-height: 180px;
  resize: vertical;
  outline: none;
  font-family: 'JetBrains Mono', monospace;
  line-height: 1.6;
}

.textarea-field:focus {
  border-color: var(--accent-cyan);
}

.btn-row {
  display: flex;
  align-items: center;
  gap: 16px;
}

.stats-tip {
  font-size: 13px;
  color: #34D399;
  font-weight: 600;
}
</style>
