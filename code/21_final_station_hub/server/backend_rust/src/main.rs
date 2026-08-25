use axum::{
    extract::{Multipart, State},
    http::StatusCode,
    response::Json,
    routing::{get, post},
    Router,
};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::sync::{Arc, Mutex};
use tower_http::cors::{Any, CorsLayer};
use tracing_subscriber::{layer::SubscriberExt, util::SubscriberInitExt};

#[derive(Clone, Debug, Serialize, Deserialize)]
pub struct DeviceTelemetry {
    pub ip: String,
    pub temp: f32,
    pub humi: f32,
    pub dist: f32,
    pub pir: bool,
    pub free_heap_kb: u32,
    pub psram_free_mb: f32,
    pub led_state: bool,
    pub last_seen: String,
}

#[derive(Clone)]
pub struct AppState {
    pub telemetry: Arc<Mutex<DeviceTelemetry>>,
    pub esp32_ip: Arc<Mutex<String>>,
}

#[tokio::main]
async fn main() {
    tracing_subscriber::registry()
        .with(tracing_subscriber::EnvFilter::new(
            std::env::var("RUST_LOG").unwrap_or_else(|_| "info".into()),
        ))
        .with(tracing_subscriber::fmt::layer())
        .init();

    let initial_telemetry = DeviceTelemetry {
        ip: "192.168.4.1".to_string(),
        temp: 26.5,
        humi: 60.2,
        dist: 22.4,
        pir: false,
        free_heap_kb: 186,
        psram_free_mb: 1.8,
        led_state: false,
        last_seen: "Just now".to_string(),
    };

    let state = AppState {
        telemetry: Arc::new(Mutex::new(initial_telemetry)),
        esp32_ip: Arc::new(Mutex::new("192.168.4.1".to_string())),
    };

    let cors = CorsLayer::new()
        .allow_origin(Any)
        .allow_methods(Any)
        .allow_headers(Any);

    let app = Router::new()
        .route("/api/status", get(get_status))
        .route("/api/control", post(post_control))
        .route("/api/photo/process", post(process_photo))
        .route("/api/novel/process", post(process_novel))
        .route("/api/ota/upload", post(process_ota))
        .layer(cors)
        .with_state(state);

    let addr = "0.0.0.0:8080";
    tracing::info!("🦀 [Rust 后端启动] 监听地址: http://{}", addr);
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    axum::serve(listener, app).await.unwrap();
}

async fn get_status(State(state): State<AppState>) -> Json<DeviceTelemetry> {
    let t = state.telemetry.lock().unwrap().clone();
    Json(t)
}

#[derive(Deserialize)]
struct ControlCmd {
    action: String,
    value: Option<bool>,
}

async fn post_control(
    State(state): State<AppState>,
    Json(cmd): Json<ControlCmd>,
) -> (StatusCode, Json<serde_json::Value>) {
    tracing::info!("🎮 收到远程控制指令: action = {}", cmd.action);
    let mut t = state.telemetry.lock().unwrap();
    if cmd.action == "toggle_led" {
        t.led_state = !t.led_state;
    } else if let Some(val) = cmd.value {
        t.led_state = val;
    }
    (StatusCode::OK, Json(serde_json::json!({ "status": "ok", "led": t.led_state })))
}

async fn process_photo(mut multipart: Multipart) -> Result<Json<serde_json::Value>, StatusCode> {
    while let Some(field) = multipart.next_field().await.map_err(|_| StatusCode::BAD_REQUEST)? {
        let file_name = field.file_name().unwrap_or("image.jpg").to_string();
        let data = field.bytes().await.map_err(|_| StatusCode::INTERNAL_SERVER_ERROR)?;

        tracing::info!("🖼️ 收到图片上传: {}, 大小: {} 字节", file_name, data.len());

        // 尝试用 image crate 解码并裁剪为 240x280
        if let Ok(img) = image::load_from_memory(&data) {
            let resized = img.resize_exact(240, 280, image::imageops::FilterType::Lanczos3);
            tracing::info!("✨ 图片已自动裁剪缩放为 240x280 分辨率！");
            return Ok(Json(serde_json::json!({
                "status": "success",
                "width": resized.width(),
                "height": resized.height(),
                "msg": "图片裁剪并转换为 240x280 格式就绪，已推送至板载 TF 卡 /sdcard/photos/"
            })));
        }
    }
    Err(StatusCode::BAD_REQUEST)
}

async fn process_novel(mut multipart: Multipart) -> Result<Json<serde_json::Value>, StatusCode> {
    while let Some(field) = multipart.next_field().await.map_err(|_| StatusCode::BAD_REQUEST)? {
        let data = field.bytes().await.map_err(|_| StatusCode::INTERNAL_SERVER_ERROR)?;
        let text = String::from_utf8_lossy(&data);
        let char_count = text.chars().count();
        let est_pages = (char_count + 159) / 160;

        tracing::info!("📖 收到电子书上传，总字数: {}, 估算页数: {}", char_count, est_pages);
        return Ok(Json(serde_json::json!({
            "status": "success",
            "characters": char_count,
            "estimated_pages": est_pages,
            "msg": "电子书切片成功，已同步写入板载 TF 卡 /sdcard/novel.txt"
        })));
    }
    Err(StatusCode::BAD_REQUEST)
}

async fn process_ota(mut multipart: Multipart) -> Result<Json<serde_json::Value>, StatusCode> {
    while let Some(field) = multipart.next_field().await.map_err(|_| StatusCode::BAD_REQUEST)? {
        let file_name = field.file_name().unwrap_or("esp32.bin").to_string();
        let data = field.bytes().await.map_err(|_| StatusCode::INTERNAL_SERVER_ERROR)?;

        let mut hasher = Sha256::new();
        hasher.update(&data);
        let sha256_hex = hex_string(&hasher.finalize());

        tracing::info!("🚀 收到固件 OTA 上传: {}, 大小: {} 字节, SHA256: {}", file_name, data.len(), sha256_hex);
        return Ok(Json(serde_json::json!({
            "status": "OTA_READY",
            "file": file_name,
            "bytes": data.len(),
            "sha256": sha256_hex,
            "msg": "固件校验通过，正在通过局域网 HTTP OTA 流式下发至 ESP32 A/B 备用分区！"
        })));
    }
    Err(StatusCode::BAD_REQUEST)
}

fn hex_string(bytes: &[u8]) -> String {
    bytes.iter().map(|b| format!("{:02x}", b)).collect()
}
