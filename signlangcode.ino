// Sign Language - ESP32 with 4 MPU6050 via PCA9548A (SPIKE DETECTION)
// Index, Middle, Ring fingers + Wrist MPU

#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>

// WiFi Credentials - CHANGE THESE! 
const char* ssid = "wifi";
const char* password = "paas";

WebServer server(80);

// I2C Addresses
const int PCA9548A_ADDR = 0x70;
const int MPU6050_ADDR = 0x68;

// PCA9548A Channels
const int CHANNEL_INDEX = 0;
const int CHANNEL_MIDDLE = 2;
const int CHANNEL_RING = 3;

// MPU Data for each sensor
struct MPUData {
  float gyroX, gyroY, gyroZ;
  float accelX, accelY, accelZ;
  int bendPercent;
  unsigned long lastBendTime;
  bool isBending;
  bool wasProcessed;
};

// Forward declarations
void updateGesture(String message, String gesture, String type, bool shouldSpeak = true);
void clearMotionHistory();
void readFingerMPU(int channel, MPUData &mpu);
void readWristMPU();
void checkFingerGestures();
void checkWristMotionGestures();
void initSingleMPU(const char* name);
void handleRoot();
void handleData();

// MPU Data instances
MPUData indexMPU = {0};
MPUData middleMPU = {0};
MPUData ringMPU = {0};
MPUData wristMPU = {0};

// Wrist motion history
float wristGyroXHistory[30];
float wristGyroYHistory[30];
float wristGyroZHistory[30];
int motionIndex = 0;
unsigned long lastMotionTime = 0;

// Current gesture info
String currentMessage = "";
String currentGesture = "";
String currentGestureType = "";
unsigned long gestureTimestamp = 0;

// Gesture detection
unsigned long lastFingerCheckTime = 0;
const unsigned long FINGER_CHECK_INTERVAL = 2000;

// Voice control - PRIORITY SYSTEM
bool isSpeaking = false;
unsigned long speechStartTime = 0;
const unsigned long MIN_SPEECH_DURATION = 3000; // 3 seconds minimum before allowing new gesture

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  delay(2000);
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   SIGN LANGUAGE - 4 MPU6050 SYSTEM   ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  Serial.print("Connecting to WiFi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\n✓ WiFi Connected!");
  Serial.print("📱 Open: http://");
  Serial.println(WiFi.localIP());
  
  initAllMPU();
  
  for (int i = 0; i < 30; i++) {
    wristGyroXHistory[i] = 0;
    wristGyroYHistory[i] = 0;
    wristGyroZHistory[i] = 0;
  }
  
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
  
  Serial.println("✓ System ready!\n");
}

void loop() {
  server.handleClient();
  
  readFingerMPU(CHANNEL_INDEX, indexMPU);
  readFingerMPU(CHANNEL_MIDDLE, middleMPU);
  readFingerMPU(CHANNEL_RING, ringMPU);
  readWristMPU();
  
  // Check if currently speaking - if yes, block new detections
  if (isSpeaking) {
    if (millis() - speechStartTime > MIN_SPEECH_DURATION) {
      isSpeaking = false;
      Serial.println("✓ Speech complete, ready for next gesture");
    }
  }
  
  // PRIORITY 1: Wrist motions (HIGHEST PRIORITY)
  if (!isSpeaking) {
    checkWristMotionGestures();
  }
  
  // PRIORITY 2: Finger gestures (only if not speaking)
  if (!isSpeaking) {
    checkFingerGestures();
  }
  
  delay(50);
}

void selectChannel(int channel) {
  Wire.beginTransmission(PCA9548A_ADDR);
  Wire.write(1 << channel);
  Wire.endTransmission();
}

void initAllMPU() {
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║     INITIALIZING MPU SENSORS          ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  selectChannel(CHANNEL_INDEX);
  initSingleMPU("INDEX");
  
  selectChannel(CHANNEL_MIDDLE);
  initSingleMPU("MIDDLE");
  
  selectChannel(CHANNEL_RING);
  initSingleMPU("RING");
  
  Wire.beginTransmission(PCA9548A_ADDR);
  Wire.write(0);
  Wire.endTransmission();
  initSingleMPU("WRIST");
  
  Serial.println("\n╔════════════════════════════════════════╗");
  Serial.println("║   ✓ ALL MPU SENSORS READY!            ║");
  Serial.println("╚════════════════════════════════════════╝\n");
}

void initSingleMPU(const char* name) {
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission(true);
  
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x1B);
  Wire.write(0x08);
  Wire.endTransmission(true);
  
  Serial.print("  ✓ ");
  Serial.print(name);
  Serial.println(" MPU initialized");
  delay(100);
}

void readFingerMPU(int channel, MPUData &mpu) {
  selectChannel(channel);
  
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 14, true);
  
  int16_t accelXRaw = Wire.read() << 8 | Wire.read();
  int16_t accelYRaw = Wire.read() << 8 | Wire.read();
  int16_t accelZRaw = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();
  int16_t gyroXRaw = Wire.read() << 8 | Wire.read();
  int16_t gyroYRaw = Wire.read() << 8 | Wire.read();
  int16_t gyroZRaw = Wire.read() << 8 | Wire.read();
  
  mpu.gyroX = gyroXRaw / 65.5;
  mpu.gyroY = gyroYRaw / 65.5;
  mpu.gyroZ = gyroZRaw / 65.5;
  
  mpu.accelX = accelXRaw / 16384.0;
  mpu.accelY = accelYRaw / 16384.0;
  mpu.accelZ = accelZRaw / 16384.0;
  
  // IMPROVED SPIKE DETECTION - Requires ACTIVE spike + decay
  float maxGyro = max(abs(mpu.gyroX), max(abs(mpu.gyroY), abs(mpu.gyroZ)));
  
  const float SPIKE_THRESHOLD = 50.0;  // Increased to 50°/s to avoid false triggers
  const unsigned long BEND_HOLD_TIME = 1500; // Hold for 1.5 seconds
  
  if (maxGyro > SPIKE_THRESHOLD) {
    // Active spike detected
    if (!mpu.isBending) {
      mpu.isBending = true;
      mpu.wasProcessed = false; // Mark as unprocessed
      mpu.lastBendTime = millis();
    }
    mpu.bendPercent = 100;
    mpu.lastBendTime = millis(); // Keep updating while spiking
  } else {
    // No spike - check decay
    unsigned long timeSinceBend = millis() - mpu.lastBendTime;
    
    if (timeSinceBend > BEND_HOLD_TIME) {
      // Decay complete
      mpu.bendPercent = 0;
      mpu.isBending = false;
      mpu.wasProcessed = false;
    } else {
      // Still holding from previous spike
      mpu.bendPercent = 100;
    }
  }
}

void readWristMPU() {
  Wire.beginTransmission(PCA9548A_ADDR);
  Wire.write(0);
  Wire.endTransmission();
  
  Wire.beginTransmission(MPU6050_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom(MPU6050_ADDR, 14, true);
  
  int16_t accelXRaw = Wire.read() << 8 | Wire.read();
  int16_t accelYRaw = Wire.read() << 8 | Wire.read();
  int16_t accelZRaw = Wire.read() << 8 | Wire.read();
  Wire.read(); Wire.read();
  int16_t gyroXRaw = Wire.read() << 8 | Wire.read();
  int16_t gyroYRaw = Wire.read() << 8 | Wire.read();
  int16_t gyroZRaw = Wire.read() << 8 | Wire.read();
  
  wristMPU.accelX = accelXRaw / 16384.0;
  wristMPU.accelY = accelYRaw / 16384.0;
  wristMPU.accelZ = accelZRaw / 16384.0;
  
  wristMPU.gyroX = gyroXRaw / 65.5;
  wristMPU.gyroY = gyroYRaw / 65.5;
  wristMPU.gyroZ = gyroZRaw / 65.5;
  
  wristGyroXHistory[motionIndex] = wristMPU.gyroX;
  wristGyroYHistory[motionIndex] = wristMPU.gyroY;
  wristGyroZHistory[motionIndex] = wristMPU.gyroZ;
  motionIndex = (motionIndex + 1) % 30;
}

void checkFingerGestures() {
  if (millis() - lastFingerCheckTime < FINGER_CHECK_INTERVAL) return;
  
  // Only trigger if bend is active AND not yet processed
  if (indexMPU.isBending && !indexMPU.wasProcessed && indexMPU.bendPercent == 100) {
    updateGesture("I am feeling sick", "SICK_INDEX", "INDEX BENT", true);
    lastFingerCheckTime = millis();
    indexMPU.wasProcessed = true;
    Serial.println("🎯 INDEX spike - Feeling sick");
    return;
  }
  
  if (middleMPU.isBending && !middleMPU.wasProcessed && middleMPU.bendPercent == 100) {
    updateGesture("I am feeling thirsty", "THIRSTY_MIDDLE", "MIDDLE BENT", true);
    lastFingerCheckTime = millis();
    middleMPU.wasProcessed = true;
    Serial.println("🎯 MIDDLE spike - Feeling thirsty");
    return;
  }
  
  if (ringMPU.isBending && !ringMPU.wasProcessed && ringMPU.bendPercent == 100) {
    updateGesture("I love you", "LOVE_RING", "RING BENT", true);
    lastFingerCheckTime = millis();
    ringMPU.wasProcessed = true;
    Serial.println("🎯 RING spike - I love you");
    return;
  }
}

void checkWristMotionGestures() {
  if (millis() - lastMotionTime < 2000) return; // Increased cooldown to 2 seconds
  
  float avgGyroX = 0, avgGyroY = 0, avgGyroZ = 0;
  float maxGyroX = -999, maxGyroY = -999, maxGyroZ = -999;
  float minGyroX = 999, minGyroY = 999, minGyroZ = 999;
  float sumAbsX = 0, sumAbsY = 0, sumAbsZ = 0;
  
  for (int i = 0; i < 30; i++) {
    avgGyroX += wristGyroXHistory[i];
    avgGyroY += wristGyroYHistory[i];
    avgGyroZ += wristGyroZHistory[i];
    
    sumAbsX += abs(wristGyroXHistory[i]);
    sumAbsY += abs(wristGyroYHistory[i]);
    sumAbsZ += abs(wristGyroZHistory[i]);
    
    if (wristGyroXHistory[i] > maxGyroX) maxGyroX = wristGyroXHistory[i];
    if (wristGyroXHistory[i] < minGyroX) minGyroX = wristGyroXHistory[i];
    if (wristGyroYHistory[i] > maxGyroY) maxGyroY = wristGyroYHistory[i];
    if (wristGyroYHistory[i] < minGyroY) minGyroY = wristGyroYHistory[i];
    if (wristGyroZHistory[i] > maxGyroZ) maxGyroZ = wristGyroZHistory[i];
    if (wristGyroZHistory[i] < minGyroZ) minGyroZ = wristGyroZHistory[i];
  }
  
  avgGyroX /= 30;
  avgGyroY /= 30;
  avgGyroZ /= 30;
  sumAbsX /= 30;
  sumAbsY /= 30;
  sumAbsZ /= 30;
  
  float rangeX = maxGyroX - minGyroX;
  float rangeY = maxGyroY - minGyroY;
  float rangeZ = maxGyroZ - minGyroZ;
  
  int changesX = 0, changesY = 0, changesZ = 0;
  for (int i = 1; i < 30; i++) {
    if ((wristGyroXHistory[i] > 0 && wristGyroXHistory[i-1] < 0) || 
        (wristGyroXHistory[i] < 0 && wristGyroXHistory[i-1] > 0)) changesX++;
    if ((wristGyroYHistory[i] > 0 && wristGyroYHistory[i-1] < 0) || 
        (wristGyroYHistory[i] < 0 && wristGyroYHistory[i-1] > 0)) changesY++;
    if ((wristGyroZHistory[i] > 0 && wristGyroZHistory[i-1] < 0) || 
        (wristGyroZHistory[i] < 0 && wristGyroZHistory[i-1] > 0)) changesZ++;
  }
  
  bool xDominant = (sumAbsX > sumAbsY) && (sumAbsX > sumAbsZ);
  bool yDominant = (sumAbsY > sumAbsX) && (sumAbsY > sumAbsZ);
  bool zDominant = (sumAbsZ > sumAbsX) && (sumAbsZ > sumAbsY);
  
  // Priority gestures - these speak
  if (xDominant && rangeX > 80 && changesX >= 2 && sumAbsX > 30) {
    updateGesture("Help Me!", "HELP_NOD", "NOD Up-Down", true);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
  
  if (yDominant && minGyroY < -40 && maxGyroY > 20 && rangeY > 60) {
    updateGesture("Hi", "HI_BOW", "BOW Forward-Back", true);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
  
  if (zDominant && rangeZ > 80 && changesZ >= 2 && sumAbsZ > 30) {
    updateGesture("My name is Vaibhav", "NAME_WAVE", "WAVE Left-Right", true);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
  
  if (rangeZ > 100 && changesZ >= 3 && sumAbsZ > 40) {
    updateGesture("How are you?", "HOW_SHAKE", "SHAKE Fast", true);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
  
  if (sumAbsX > 40 && sumAbsY > 40 && sumAbsZ > 40 && 
      rangeX > 80 && rangeY > 80 && rangeZ > 80 &&
      changesX >= 2 && changesY >= 2 && changesZ >= 2) {
    updateGesture("EMERGENCY! Call 911!", "EMERGENCY_CIRCLE", "RAPID CIRCLE", true);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
  
  if (minGyroX < -100 && changesX >= 2 && rangeX > 120 && sumAbsX > 50) {
    updateGesture("CALL HELP!", "CALL_HELP_JERK", "SHARP UP JERKS", true);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
  
  if (changesX >= 2 && changesY >= 2 && 
      rangeX > 40 && rangeX < 90 && rangeY > 40 && rangeY < 90) {
    updateGesture("Maybe", "MAYBE_WOBBLE", "WOBBLE", false);
    lastMotionTime = millis();
    clearMotionHistory();
    return;
  }
}

void clearMotionHistory() {
  for (int i = 0; i < 30; i++) {
    wristGyroXHistory[i] = 0;
    wristGyroYHistory[i] = 0;
    wristGyroZHistory[i] = 0;
  }
}

void updateGesture(String message, String gesture, String type, bool shouldSpeak) {
  currentMessage = message;
  currentGesture = gesture;
  currentGestureType = type;
  gestureTimestamp = millis();
  
  if (shouldSpeak) {
    currentGestureType = type + "|SPEAK";
    isSpeaking = true;
    speechStartTime = millis();
    Serial.println("🔊 Speaking started - blocking new gestures");
  } else {
    currentGestureType = type + "|SILENT";
  }
  
  Serial.print("🗣  GESTURE: ");
  Serial.print(message);
  Serial.print(" -> ");
  Serial.print(type);
  if (!shouldSpeak) {
    Serial.print(" (TEXT ONLY)");
  }
  Serial.println();
}

void handleRoot() {
  String html = "<!DOCTYPE html><html><head><meta charset='UTF-8'><meta name='viewport' content='width=device-width,initial-scale=1.0'><title>Sign Language</title><style>*{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);min-height:100vh;padding:15px;display:flex;flex-direction:column;align-items:center}.container{width:100%;max-width:500px;background:white;border-radius:20px;padding:25px;box-shadow:0 20px 60px rgba(0,0,0,0.3)}h1{text-align:center;color:#667eea;margin-bottom:5px;font-size:24px}.subtitle{text-align:center;color:#666;margin-bottom:20px;font-size:13px}.speech-box{background:linear-gradient(135deg,#667eea 0%,#764ba2 100%);color:white;padding:30px 20px;border-radius:15px;text-align:center;margin-bottom:20px;min-height:180px;display:flex;flex-direction:column;align-items:center;justify-content:center;box-shadow:0 10px 30px rgba(102,126,234,0.3)}.speech-box.speaking{animation:pulse 0.5s ease-in-out;box-shadow:0 0 30px rgba(255,215,0,0.6)}@keyframes pulse{0%,100%{transform:scale(1)}50%{transform:scale(1.05)}}.emoji{font-size:60px;margin-bottom:15px}.message{font-size:28px;font-weight:bold;margin-bottom:8px}.gesture-type{font-size:14px;opacity:0.9;margin-top:5px}.status{background:#e8f5e9;padding:12px;border-radius:10px;text-align:center;color:#2e7d32;margin-bottom:15px;font-weight:600;font-size:14px}.status.speaking{background:#fff3e0;color:#e65100}.finger-grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:15px}.finger-card{background:#f8f9fa;padding:12px;border-radius:10px;border:2px solid #e0e0e0}.finger-name{font-weight:bold;color:#667eea;margin-bottom:6px;font-size:12px}.finger-percent{font-size:18px;font-weight:bold;color:#333}.progress-bar{width:100%;height:8px;background:#e0e0e0;border-radius:5px;overflow:hidden;margin-top:6px}.progress-fill{height:100%;background:linear-gradient(90deg,#667eea 0%,#764ba2 100%);transition:width 0.3s ease}.mpu-data{background:#fff3e0;padding:12px;border-radius:10px;margin-bottom:15px;font-size:12px;color:#e65100}.test-voice-btn{width:100%;padding:14px;background:#4CAF50;color:white;border:none;border-radius:10px;font-size:15px;font-weight:bold;cursor:pointer}</style></head><body><div class='container'><h1>🤟 Sign Language</h1><div class='subtitle'>Priority Speech System</div><div class='speech-box' id='speechBox'><div class='emoji' id='emoji'>👋</div><div class='message' id='message'>Waiting...</div><div class='gesture-type' id='gestureType'></div></div><div class='status' id='status'>Connected</div><div class='finger-grid'><div class='finger-card'><div class='finger-name'>👆 INDEX</div><div class='finger-percent' id='percent0'>0%</div><div class='progress-bar'><div class='progress-fill' id='progress0'></div></div></div><div class='finger-card'><div class='finger-name'>🖕 MIDDLE</div><div class='finger-percent' id='percent1'>0%</div><div class='progress-bar'><div class='progress-fill' id='progress1'></div></div></div><div class='finger-card'><div class='finger-name'>💍 RING</div><div class='finger-percent' id='percent2'>0%</div><div class='progress-bar'><div class='progress-fill' id='progress2'></div></div></div><div class='finger-card'><div class='finger-name'>🤙 PINKY</div><div class='finger-percent' id='percent3'>0%</div><div class='progress-bar'><div class='progress-fill' id='progress3'></div></div></div></div><div class='mpu-data' id='mpuData'>Wrist Motion: X:0 Y:0 Z:0</div><button class='test-voice-btn' onclick='testVoice()'>🔊 Test Voice</button></div><script>let lastTimestamp=0;let speechSynth=window.speechSynthesis;let voiceReady=false;let currentlySpeaking=false;function initSpeech(){document.addEventListener('click',function(){if(!voiceReady){let u=new SpeechSynthesisUtterance('');u.volume=0;speechSynth.speak(u);setTimeout(()=>{if(speechSynth.getVoices().length>0){voiceReady=true;console.log('Voice ready')}},100)}},{once:false});if(speechSynth.getVoices().length>0){voiceReady=true}else{speechSynth.onvoiceschanged=()=>{voiceReady=true}}}initSpeech();const emojiMap={'SICK_INDEX':'🤒','THIRSTY_MIDDLE':'💧','LOVE_RING':'❤️','HELP_NOD':'🆘','HI_BOW':'👋','NAME_WAVE':'🙋‍♂️','HOW_SHAKE':'❓','EMERGENCY_CIRCLE':'🚨','CALL_HELP_JERK':'🚨','MAYBE_WOBBLE':'🤷'};function updateData(){fetch('/data').then(r=>r.json()).then(data=>{document.getElementById('percent0').textContent=data.bend[0]+'%';document.getElementById('progress0').style.width=data.bend[0]+'%';document.getElementById('percent1').textContent=data.bend[1]+'%';document.getElementById('progress1').style.width=data.bend[1]+'%';document.getElementById('percent2').textContent=data.bend[2]+'%';document.getElementById('progress2').style.width=data.bend[2]+'%';document.getElementById('percent3').textContent='0%';document.getElementById('progress3').style.width='0%';document.getElementById('mpuData').textContent='Wrist: X:'+data.gyro[0]+' Y:'+data.gyro[1]+' Z:'+data.gyro[2];if(currentlySpeaking){document.getElementById('status').className='status speaking';document.getElementById('status').textContent='🔊 Speaking...'}else{document.getElementById('status').className='status';document.getElementById('status').textContent='✓ Ready'}if(data.message&&data.timestamp>lastTimestamp){lastTimestamp=data.timestamp;speakGesture(data.message,data.gesture,data.type)}}).catch(e=>console.log('Fetch error:',e))}function speakGesture(message,gesture,type){document.getElementById('message').textContent=message;document.getElementById('emoji').textContent=emojiMap[gesture]||'🤟';document.getElementById('gestureType').textContent=type.replace('|SPEAK','').replace('|SILENT','');document.getElementById('speechBox').classList.remove('speaking');setTimeout(()=>document.getElementById('speechBox').classList.add('speaking'),10);if(type.includes('|SPEAK')){speakText(message)}}function speakText(text){if(currentlySpeaking){console.log('Already speaking');return}if(speechSynth.speaking)speechSynth.cancel();setTimeout(()=>{currentlySpeaking=true;let u=new SpeechSynthesisUtterance(text);u.rate=0.85;u.lang='en-US';u.onstart=()=>{console.log('Speech started');document.getElementById('status').className='status speaking';document.getElementById('status').textContent='🔊 Speaking...'};u.onend=()=>{console.log('Speech ended');currentlySpeaking=false;document.getElementById('status').className='status';document.getElementById('status').textContent='✓ Ready'};u.onerror=()=>{console.log('Speech error');currentlySpeaking=false};speechSynth.speak(u)},100)}function testVoice(){if(!voiceReady){alert('Click anywhere first');return}speakText('Voice working perfectly')}setInterval(updateData,200);updateData()</script></body></html>";
  
  server.send(200, "text/html", html);
}

void handleData() {
  String json = "{";
  json += "\"bend\":[" + String(indexMPU.bendPercent) + "," + String(middleMPU.bendPercent) + "," + String(ringMPU.bendPercent) + "],";
  json += "\"gyro\":[" + String((int)wristMPU.gyroX) + "," + String((int)wristMPU.gyroY) + "," + String((int)wristMPU.gyroZ) + "],";
  json += "\"message\":\"" + currentMessage + "\",";
  json += "\"gesture\":\"" + currentGesture + "\",";
  json += "\"type\":\"" + currentGestureType + "\",";
  json += "\"timestamp\":" + String(gestureTimestamp);
  json += "}";
  
  server.send(200, "application/json", json);
  
  Serial.print("📡 Data sent: ");
  Serial.println(json);
}