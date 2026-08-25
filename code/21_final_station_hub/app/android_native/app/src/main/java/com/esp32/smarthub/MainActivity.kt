package com.esp32.smarthub

import android.Manifest
import android.bluetooth.*
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanResult
import android.content.Context
import android.content.pm.PackageManager
import android.os.Build
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import java.util.*

class MainActivity : AppCompatActivity() {

    private var bluetoothAdapter: BluetoothAdapter? = null
    private var bluetoothGatt: BluetoothGatt? = null

    private lateinit var tvStatus: TextView
    private lateinit var tvTemp: TextView
    private lateinit var tvHumi: TextView
    private lateinit var tvDist: TextView
    private lateinit var btnScan: Button
    private lateinit var btnToggleLed: Button

    private val SERVICE_UUID = UUID.fromString("000000ff-0000-1000-8000-00805f9b34fb")
    private val CHAR_SENSOR_UUID = UUID.fromString("0000ff01-0000-1000-8000-00805f9b34fb")
    private val CHAR_CTRL_UUID = UUID.fromString("0000ff02-0000-1000-8000-00805f9b34fb")

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        tvStatus = findViewById(R.id.tvStatus)
        tvTemp = findViewById(R.id.tvTemp)
        tvHumi = findViewById(R.id.tvHumi)
        tvDist = findViewById(R.id.tvDist)
        btnScan = findViewById(R.id.btnScan)
        btnToggleLed = findViewById(R.id.btnToggleLed)

        val bluetoothManager = getSystemService(Context.BLUETOOTH_SERVICE) as BluetoothManager
        bluetoothAdapter = bluetoothManager.adapter

        requestPermissions()

        btnScan.setOnClickListener {
            startBleScan()
        }

        btnToggleLed.setOnClickListener {
            sendBleCommand("TOGGLE_LED")
        }
    }

    private fun requestPermissions() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ActivityCompat.requestPermissions(
                this,
                arrayOf(
                    Manifest.permission.BLUETOOTH_SCAN,
                    Manifest.permission.BLUETOOTH_CONNECT,
                    Manifest.permission.ACCESS_FINE_LOCATION
                ), 1
            )
        }
    }

    private fun startBleScan() {
        tvStatus.text = "🔍 正在扫描 ESP32-Smart-Hub..."
        val scanner = bluetoothAdapter?.bluetoothLeScanner
        scanner?.startScan(object : ScanCallback() {
            override fun onScanResult(callbackType: Int, result: ScanResult?) {
                val device = result?.device
                if (device?.name == "ESP32-Smart-Hub") {
                    scanner.stopScan(this)
                    tvStatus.text = "🟢 发现设备，正在连接 GATT..."
                    connectToDevice(device)
                }
            }
        })
    }

    private fun connectToDevice(device: BluetoothDevice) {
        bluetoothGatt = device.connectGatt(this, false, object : BluetoothGattCallback() {
            override fun onConnectionStateChange(gatt: BluetoothGatt?, status: Int, newState: Int) {
                if (newState == BluetoothProfile.STATE_CONNECTED) {
                    runOnUiThread { tvStatus.text = "✅ 已通过 BLE 连接到 ESP32 中控台" }
                    gatt?.discoverServices()
                } else if (newState == BluetoothProfile.STATE_DISCONNECTED) {
                    runOnUiThread { tvStatus.text = "⚠️ BLE 蓝牙已断开" }
                }
            }

            override fun onServicesDiscovered(gatt: BluetoothGatt?, status: Int) {
                val service = gatt?.getService(SERVICE_UUID)
                val sensorChar = service?.getCharacteristic(CHAR_SENSOR_UUID)
                if (sensorChar != null) {
                    gatt.setCharacteristicNotification(sensorChar, true)
                }
            }

            override fun onCharacteristicChanged(gatt: BluetoothGatt?, characteristic: BluetoothGattCharacteristic?) {
                val data = characteristic?.value?.let { String(it) } ?: return
                runOnUiThread {
                    // 解析 T:26.5,H:60.0,D:22.4
                    val parts = data.split(",")
                    for (part in parts) {
                        if (part.startsWith("T:")) tvTemp.text = "${part.substring(2)} °C"
                        if (part.startsWith("H:")) tvHumi.text = "${part.substring(2)} %"
                        if (part.startsWith("D:")) tvDist.text = "${part.substring(2)} cm"
                    }
                }
            }
        })
    }

    private fun sendBleCommand(cmd: String) {
        val service = bluetoothGatt?.getService(SERVICE_UUID)
        val ctrlChar = service?.getCharacteristic(CHAR_CTRL_UUID)
        if (ctrlChar != null) {
            ctrlChar.value = cmd.toByteArray()
            bluetoothGatt?.writeCharacteristic(ctrlChar)
            Toast.makeText(this, "已发送指令: $cmd", Toast.LENGTH_SHORT).show()
        }
    }
}
