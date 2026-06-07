#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <WebServer.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "BoardPins.h"
#include "mbedtls/base64.h"

namespace {

constexpr char kDeviceName[] = "PrintSphere";
constexpr char kSetupPassword[] = "printsphere";
constexpr char kLoginPath[] = "/v1/user-service/user/login";
constexpr char kEmailCodePath[] = "/v1/user-service/user/sendemail/code";
constexpr char kSmsCodePath[] = "/v1/user-service/user/sendsmscode";
constexpr char kBindPath[] = "/v1/iot-service/api/user/bind";
constexpr char kPreferencePath[] = "/v1/design-user-service/my/preference";
constexpr char kGetVersion[] = "{\"info\":{\"sequence_id\":\"0\",\"command\":\"get_version\"}}";
constexpr char kPushAll[] = "{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\"}}";
constexpr uint16_t kCloudMqttPort = 8883;
constexpr uint32_t kCloudTaskStackBytes = 12288;
constexpr uint32_t kDisplayTaskStackBytes = 8192;
constexpr uint32_t kPortalTaskStackBytes = 8192;
constexpr uint32_t kMqttBufferBytes = 32768;
constexpr uint32_t kBusyTimeoutMs = 120000;
constexpr uint8_t kFullRefreshControl = 0xF7;
constexpr uint8_t kPartialRefreshControl = 0xFC;
constexpr uint8_t kMaxPartialBeforeFull = 5;
constexpr int kMaxPartialPixels = (EPAPER_WIDTH * EPAPER_HEIGHT) / 2;
constexpr char kShanghaiTimezone[] = "CST-8";

constexpr char kGlobalSignRootR3[] = R"CERT(
-----BEGIN CERTIFICATE-----
MIIETjCCAzagAwIBAgINAe5fFp3/lzUrZGXWajANBgkqhkiG9w0BAQsFADBXMQsw
CQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UECxMH
Um9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTE4MDkxOTAw
MDAwMFoXDTI4MDEyODEyMDAwMFowTDEgMB4GA1UECxMXR2xvYmFsU2lnbiBSb290
IENBIC0gUjMxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNp
Z24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDMJXaQeQZ4Ihb1wIO2
hMoonv0FdhHFrYhy/EYCQ8eyip0EXyTLLkvhYIJG4VKrDIFHcGzdZNHr9SyjD4I9
DCuul9e2FIYQebs7E4B3jAjhSdJqYi8fXvqWaN+JJ5U4nwbXPsnLJlkNc96wyOkm
DoMVxu9bi9IEYMpJpij2aTv2y8gokeWdimFXN6x0FNx04Druci8unPvQu7/1PQDh
BjPogiuuU6Y6FnOM3UEOIDrAtKeh6bJPkC4yYOlXy7kEkmho5TgmYHWyn3f/kRTv
riBJ/K1AFUjRAjFhGV64l++td7dkmnq/X8ET75ti+w1s4FRpFqkD2m7pg5NxdsZp
hYIXAgMBAAGjggEiMIIBHjAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB
/zAdBgNVHQ4EFgQUj/BLf6guRSSuTVD6Y5qL3uLdG7wwHwYDVR0jBBgwFoAUYHtm
GkUNl8qJUC99BM00qP/8/UswPQYIKwYBBQUHAQEEMTAvMC0GCCsGAQUFBzABhiFo
dHRwOi8vb2NzcC5nbG9iYWxzaWduLmNvbS9yb290cjEwMwYDVR0fBCwwKjAooCag
JIYiaHR0cDovL2NybC5nbG9iYWxzaWduLmNvbS9yb290LmNybDBHBgNVHSAEQDA+
MDwGBFUdIAAwNDAyBggrBgEFBQcCARYmaHR0cHM6Ly93d3cuZ2xvYmFsc2lnbi5j
b20vcmVwb3NpdG9yeS8wDQYJKoZIhvcNAQELBQADggEBACNw6c/ivvVZrpRCb8RD
M6rNPzq5ZBfyYgZLSPFAiAYXof6r0V88xjPy847dHx0+zBpgmYILrMf8fpqHKqV9
D6ZX7qw7aoXW3r1AY/itpsiIsBL89kHfDwmXHjjqU5++BfQ+6tOfUBJ2vgmLwgtI
fR4uUfaNU9OrH0Abio7tfftPeVZwXwzTjhuzp3ANNyuXlava4BJrHEDOxcd+7cJi
WOx37XMiwor1hkOIreoTbv3Y/kIvuX1erRjvlJDKPSerJpSZdcfL03v3ykzTr1Eh
kluEfSufFT90y1HonoMOFm8b50bOI7355KKL0jlrqnkckSziYSQtjipIcJDEHsXo
4HA=
-----END CERTIFICATE-----
)CERT";

constexpr char kCloudCaBundle[] = R"CERT(
-----BEGIN CERTIFICATE-----
MIIETjCCAzagAwIBAgINAe5fFp3/lzUrZGXWajANBgkqhkiG9w0BAQsFADBXMQsw
CQYDVQQGEwJCRTEZMBcGA1UEChMQR2xvYmFsU2lnbiBudi1zYTEQMA4GA1UECxMH
Um9vdCBDQTEbMBkGA1UEAxMSR2xvYmFsU2lnbiBSb290IENBMB4XDTE4MDkxOTAw
MDAwMFoXDTI4MDEyODEyMDAwMFowTDEgMB4GA1UECxMXR2xvYmFsU2lnbiBSb290
IENBIC0gUjMxEzARBgNVBAoTCkdsb2JhbFNpZ24xEzARBgNVBAMTCkdsb2JhbFNp
Z24wggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDMJXaQeQZ4Ihb1wIO2
hMoonv0FdhHFrYhy/EYCQ8eyip0EXyTLLkvhYIJG4VKrDIFHcGzdZNHr9SyjD4I9
DCuul9e2FIYQebs7E4B3jAjhSdJqYi8fXvqWaN+JJ5U4nwbXPsnLJlkNc96wyOkm
DoMVxu9bi9IEYMpJpij2aTv2y8gokeWdimFXN6x0FNx04Druci8unPvQu7/1PQDh
BjPogiuuU6Y6FnOM3UEOIDrAtKeh6bJPkC4yYOlXy7kEkmho5TgmYHWyn3f/kRTv
riBJ/K1AFUjRAjFhGV64l++td7dkmnq/X8ET75ti+w1s4FRpFqkD2m7pg5NxdsZp
hYIXAgMBAAGjggEiMIIBHjAOBgNVHQ8BAf8EBAMCAQYwDwYDVR0TAQH/BAUwAwEB
/zAdBgNVHQ4EFgQUj/BLf6guRSSuTVD6Y5qL3uLdG7wwHwYDVR0jBBgwFoAUYHtm
GkUNl8qJUC99BM00qP/8/UswPQYIKwYBBQUHAQEEMTAvMC0GCCsGAQUFBzABhiFo
dHRwOi8vb2NzcC5nbG9iYWxzaWduLmNvbS9yb290cjEwMwYDVR0fBCwwKjAooCag
JIYiaHR0cDovL2NybC5nbG9iYWxzaWduLmNvbS9yb290LmNybDBHBgNVHSAEQDA+
MDwGBFUdIAAwNDAyBggrBgEFBQcCARYmaHR0cHM6Ly93d3cuZ2xvYmFsc2lnbi5j
b20vcmVwb3NpdG9yeS8wDQYJKoZIhvcNAQELBQADggEBACNw6c/ivvVZrpRCb8RD
M6rNPzq5ZBfyYgZLSPFAiAYXof6r0V88xjPy847dHx0+zBpgmYILrMf8fpqHKqV9
D6ZX7qw7aoXW3r1AY/itpsiIsBL89kHfDwmXHjjqU5++BfQ+6tOfUBJ2vgmLwgtI
fR4uUfaNU9OrH0Abio7tfftPeVZwXwzTjhuzp3ANNyuXlava4BJrHEDOxcd+7cJi
WOx37XMiwor1hkOIreoTbv3Y/kIvuX1erRjvlJDKPSerJpSZdcfL03v3ykzTr1Eh
kluEfSufFT90y1HonoMOFm8b50bOI7355KKL0jlrqnkckSziYSQtjipIcJDEHsXo
4HA=
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIEfjCCA2agAwIBAgIQD+Ayq4RNAzEGxQyOE8iwaDANBgkqhkiG9w0BAQsFADBh
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBD
QTAeFw0yNDAxMTgwMDAwMDBaFw0zMTExMDkyMzU5NTlaMGExCzAJBgNVBAYTAlVT
MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j
b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG
9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI
2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx
1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ
q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz
tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ
vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo4IBMDCC
ASwwDwYDVR0TAQH/BAUwAwEB/zAdBgNVHQ4EFgQUTiJUIBiV5uNu5g/6+rkS7QYX
jzkwHwYDVR0jBBgwFoAUA95QNVbRTLtm8KPiGxvDl7I90VUwDgYDVR0PAQH/BAQD
AgGGMHQGCCsGAQUFBwEBBGgwZjAjBggrBgEFBQcwAYYXaHR0cDovL29jc3AuZGln
aWNlcnQuY24wPwYIKwYBBQUHMAKGM2h0dHA6Ly9jYWNlcnRzLmRpZ2ljZXJ0LmNu
L0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNydDBABgNVHR8EOTA3MDWgM6Axhi9odHRw
Oi8vY3JsLmRpZ2ljZXJ0LmNuL0RpZ2lDZXJ0R2xvYmFsUm9vdENBLmNybDARBgNV
HSAECjAIMAYGBFUdIAAwDQYJKoZIhvcNAQELBQADggEBAHRBl3jN7+XHBUK0dZnu
hMdoNwD1nCROU3BTIh1TNzRI0bQ0m5+C/dCRzzlqoSAFHUlOi+OiDltWkXTzmQn6
Z8bH5PFBy5sYpc/8cNPoSzhyqcpvvEZvv/Ivc0Up+dzma7vBDJC9WrMRUUlSFSQp
kdXSmphDNkXJsgARmxzc18IN6LYMRiOWlY7RE2F900pPW60BvJHHNCX0bbSRj/Ql
bmVq8wuftBD++D+RS8K++ujpMjFBROyWfBX+woQDGsMazkmgulQdnZrdj476elOL
axRvrSgEorju1kJM7M65z2RUZrfzQYW/1rs8mRUXin6iEtad/Rv1ZI1WGYmWPyBm
pbo=
-----END CERTIFICATE-----
)CERT";

enum class CloudRegion : uint8_t { kUS, kEU, kCN };
enum class PrintLifecycleState : uint8_t {
  kUnknown,
  kIdle,
  kPreparing,
  kPrinting,
  kPaused,
  kFinished,
  kError,
};
enum class PrinterConnectionState : uint8_t {
  kBooting,
  kWaitingForCredentials,
  kConnecting,
  kOnline,
  kError,
};

struct WifiCredentials {
  std::string ssid;
  std::string password;
  bool is_configured() const { return !ssid.empty(); }
};

struct BambuCloudCredentials {
  std::string email;
  std::string password;
  CloudRegion region = CloudRegion::kCN;
  bool has_identity() const { return !email.empty(); }
  bool can_password_login() const { return has_identity() && !password.empty(); }
};

struct PrinterConnection {
  std::string serial;
  bool is_ready() const { return !serial.empty(); }
};

struct PrinterSnapshot {
  PrinterConnectionState connection = PrinterConnectionState::kBooting;
  PrintLifecycleState lifecycle = PrintLifecycleState::kUnknown;
  std::string stage = "boot";
  std::string detail = "Starting up";
  std::string raw_status;
  std::string raw_stage;
  std::string job_name;
  std::string resolved_serial;
  float progress_percent = 0.0f;
  uint32_t remaining_seconds = 0;
  uint16_t current_layer = 0;
  uint16_t total_layers = 0;
  float nozzle_temp_c = 0.0f;
  bool nozzle_temp_known = false;
  float bed_temp_c = 0.0f;
  bool bed_temp_known = false;
  bool has_error = false;
  bool wifi_connected = false;
  std::string wifi_ip;
  bool setup_ap_active = true;
  std::string setup_ap_ssid = std::string(kDeviceName) + "-Setup";
  std::string setup_ap_ip = "192.168.4.1";
  bool cloud_configured = false;
  bool cloud_connected = false;
};

struct EpaperStatusLines {
  std::string status;
  std::string detail;
  std::string progress;
  std::string remaining;
  std::string temperatures;
  std::string layers;
  std::string network;
  std::string clock;
};

SemaphoreHandle_t gStateMutex = nullptr;
PrinterSnapshot gSnapshot;

std::string to_std(const String& text) { return std::string(text.c_str()); }

void start_time_sync() {
  configTzTime(kShanghaiTimezone, "pool.ntp.org", "time.google.com", "ntp.aliyun.com");
}

std::string compact_response_preview(const std::string& response) {
  if (response.empty()) {
    return "empty body";
  }
  if (response.size() >= 2U &&
      static_cast<uint8_t>(response[0]) == 0x1F &&
      static_cast<uint8_t>(response[1]) == 0x8B) {
    return "gzip body";
  }

  std::string out;
  const size_t max_len = std::min<size_t>(response.size(), 48U);
  out.reserve(max_len + 2);
  for (size_t i = 0; i < max_len; ++i) {
    const unsigned char ch = static_cast<unsigned char>(response[i]);
    if (ch == '\r' || ch == '\n' || ch == '\t') {
      out.push_back(' ');
    } else if (std::isprint(ch) != 0) {
      out.push_back(static_cast<char>(ch));
    } else {
      out.push_back('.');
    }
  }
  return out;
}

String to_string(CloudRegion region) {
  switch (region) {
    case CloudRegion::kUS:
      return "us";
    case CloudRegion::kCN:
      return "cn";
    case CloudRegion::kEU:
    default:
      return "eu";
  }
}

CloudRegion parse_region(const String& value) {
  if (value == "us") return CloudRegion::kUS;
  if (value == "cn") return CloudRegion::kCN;
  return CloudRegion::kEU;
}

std::string uppercase_ascii(std::string text) {
  for (char& c : text) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
  return text;
}

std::string normalize_bambu_status_token(const std::string& status_text) {
  std::string normalized;
  normalized.reserve(status_text.size());
  for (char ch : status_text) {
    if (std::isalnum(static_cast<unsigned char>(ch)) != 0) {
      normalized.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
    }
  }
  return normalized;
}

bool bambu_status_is_failed(const std::string& status_text) {
  const std::string n = normalize_bambu_status_token(status_text);
  return n.find("FAIL") != std::string::npos || n.find("ERROR") != std::string::npos ||
         n.find("CANCEL") != std::string::npos;
}

bool bambu_status_is_finished(const std::string& status_text) {
  const std::string n = normalize_bambu_status_token(status_text);
  return n.find("DONE") != std::string::npos || n.find("SUCCESS") != std::string::npos ||
         n.find("COMPLETE") != std::string::npos || n.find("COMPLETED") != std::string::npos ||
         n.find("FINISH") != std::string::npos;
}

PrintLifecycleState lifecycle_from_bambu_status(const std::string& status_text) {
  const std::string n = normalize_bambu_status_token(status_text);
  if (n.empty() || n == "UNKNOWN") return PrintLifecycleState::kUnknown;
  if (bambu_status_is_failed(n)) return PrintLifecycleState::kError;
  if (n.find("PAUSE") != std::string::npos) return PrintLifecycleState::kPaused;
  if (n == "INIT" || n == "SLICING" || n.find("PREPARE") != std::string::npos ||
      n.find("PREPARING") != std::string::npos || n.find("STARTING") != std::string::npos ||
      n.find("HEATING") != std::string::npos || n.find("DOWNLOAD") != std::string::npos) {
    return PrintLifecycleState::kPreparing;
  }
  if (n.find("RUNNING") != std::string::npos || n.find("PRINTING") != std::string::npos ||
      n.find("PROCESSING") != std::string::npos) {
    return PrintLifecycleState::kPrinting;
  }
  if (bambu_status_is_finished(n)) return PrintLifecycleState::kFinished;
  if (n.find("IDLE") != std::string::npos || n.find("WAIT") != std::string::npos ||
      n.find("OFFLINE") != std::string::npos) {
    return PrintLifecycleState::kIdle;
  }
  return PrintLifecycleState::kUnknown;
}

std::string default_stage_label_for_status(const std::string& status_text) {
  const std::string n = normalize_bambu_status_token(status_text);
  if (n.find("DOWNLOAD") != std::string::npos) return "downloading";
  if (bambu_status_is_failed(n)) return "failed";
  if (bambu_status_is_finished(n)) return "done";
  if (n.find("PAUSE") != std::string::npos) return "paused";
  if (n.find("PREPARE") != std::string::npos || n.find("HEATING") != std::string::npos) {
    return "preparing";
  }
  if (n.find("RUNNING") != std::string::npos || n.find("PRINTING") != std::string::npos) {
    return "printing";
  }
  if (n.find("IDLE") != std::string::npos || n.find("WAIT") != std::string::npos) return "idle";
  return status_text.empty() ? "Status" : status_text;
}

template <typename T>
std::string json_string(T object, const char* key, const std::string& fallback = {}) {
  JsonVariantConst item = object[key];
  if (item.is<const char*>()) return item.as<const char*>();
  if (item.is<String>()) return to_std(item.as<String>());
  return fallback;
}

template <typename T>
int json_int(T object, const char* key, int fallback = -1) {
  JsonVariantConst item = object[key];
  if (item.is<int>()) return item.as<int>();
  if (item.is<const char*>()) return std::strtol(item.as<const char*>(), nullptr, 0);
  return fallback;
}

template <typename T>
float json_float(T object, const char* key, float fallback = -1.0f) {
  JsonVariantConst item = object[key];
  if (item.is<float>() || item.is<double>() || item.is<int>()) return item.as<float>();
  if (item.is<const char*>()) return std::strtof(item.as<const char*>(), nullptr);
  return fallback;
}

class ConfigStore {
 public:
  bool begin() { return prefs_.begin("printsphere", false); }

  WifiCredentials load_wifi() {
    return {to_std(prefs_.getString("wifi_ssid", "")), to_std(prefs_.getString("wifi_pass", ""))};
  }

  BambuCloudCredentials load_cloud() {
    BambuCloudCredentials c;
    c.email = to_std(prefs_.getString("cloud_email", ""));
    c.password = to_std(prefs_.getString("cloud_pass", ""));
    c.region = parse_region(prefs_.getString("cloud_region", "cn"));
    return c;
  }

  PrinterConnection load_printer() {
    PrinterConnection c;
    c.serial = to_std(prefs_.getString("prn_serial", ""));
    if (c.serial.empty()) c.serial = to_std(prefs_.getString("prn_0_ser", ""));
    return c;
  }

  std::string load_token() { return to_std(prefs_.getString("cloud_token", "")); }

  void save_wifi(const WifiCredentials& c) {
    prefs_.putString("wifi_ssid", c.ssid.c_str());
    prefs_.putString("wifi_pass", c.password.c_str());
  }

  void save_cloud(const BambuCloudCredentials& c) {
    prefs_.putString("cloud_email", c.email.c_str());
    prefs_.putString("cloud_pass", c.password.c_str());
    prefs_.putString("cloud_region", to_string(c.region));
  }

  void save_printer(const PrinterConnection& c) {
    prefs_.putString("prn_serial", c.serial.c_str());
    prefs_.putString("prn_0_ser", c.serial.c_str());
    prefs_.putUChar("prn_count", c.serial.empty() ? 0 : 1);
    prefs_.putUChar("prn_active", 0);
  }

  void save_token(const std::string& token) { prefs_.putString("cloud_token", token.c_str()); }
  void clear_token() { prefs_.remove("cloud_token"); }

 private:
  Preferences prefs_;
};

ConfigStore gConfig;

PrinterSnapshot read_snapshot() {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  PrinterSnapshot copy = gSnapshot;
  xSemaphoreGive(gStateMutex);
  return copy;
}

void mutate_snapshot(const std::function<void(PrinterSnapshot&)>& fn) {
  xSemaphoreTake(gStateMutex, portMAX_DELAY);
  fn(gSnapshot);
  xSemaphoreGive(gStateMutex);
}

class EpaperCanvas {
 public:
  EpaperCanvas(int width, int height)
      : width_(width), height_(height), bytes_per_row_((width + 7) / 8),
        buffer_(static_cast<size_t>(bytes_per_row_ * height_), 0xFF) {}

  int width() const { return width_; }
  int height() const { return height_; }
  const std::vector<uint8_t>& buffer() const { return buffer_; }
  void clear() { std::fill(buffer_.begin(), buffer_.end(), 0xFF); }

  void set_pixel(int x, int y, bool black) {
    if (x < 0 || y < 0 || x >= width_ || y >= height_) return;
    const size_t idx = static_cast<size_t>(y * bytes_per_row_ + x / 8);
    const uint8_t mask = static_cast<uint8_t>(0x80U >> (x & 7));
    if (black) buffer_[idx] &= static_cast<uint8_t>(~mask);
    else buffer_[idx] |= mask;
  }

  void fill_rect(int x, int y, int w, int h, bool black) {
    for (int yy = y; yy < y + h; ++yy) {
      for (int xx = x; xx < x + w; ++xx) set_pixel(xx, yy, black);
    }
  }

  void draw_text(int x, int y, const std::string& text, int scale = 1) {
    int cursor = x;
    for (char c : text) {
      draw_char(cursor, y, c, scale);
      cursor += 6 * scale;
    }
  }

  void draw_right_text(int right_x, int y, const std::string& text, int scale = 1) {
    draw_text(right_x - text_width(text, scale), y, text, scale);
  }

  int text_width(const std::string& text, int scale = 1) const {
    if (text.empty()) return 0;
    return static_cast<int>(text.size()) * 6 * scale - scale;
  }

 private:
  std::array<uint8_t, 7> glyph_rows(char input) {
    const char c = static_cast<char>(std::toupper(static_cast<unsigned char>(input)));
    switch (c) {
      case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
      case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
      case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
      case 'D': return {0x1E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1E};
      case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
      case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
      case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E};
      case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
      case 'I': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x1F};
      case 'J': return {0x01, 0x01, 0x01, 0x01, 0x11, 0x11, 0x0E};
      case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
      case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
      case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
      case 'N': return {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11};
      case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
      case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
      case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
      case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
      case 'S': return {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E};
      case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
      case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
      case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
      case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0A};
      case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
      case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
      case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
      case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
      case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
      case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
      case '3': return {0x1E, 0x01, 0x01, 0x0E, 0x01, 0x01, 0x1E};
      case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
      case '5': return {0x1F, 0x10, 0x10, 0x1E, 0x01, 0x01, 0x1E};
      case '6': return {0x0E, 0x10, 0x10, 0x1E, 0x11, 0x11, 0x0E};
      case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
      case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
      case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x01, 0x0E};
      case '%': return {0x19, 0x19, 0x02, 0x04, 0x08, 0x13, 0x13};
      case '/': return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
      case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
      case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
      case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
      case '_': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1F};
      case ' ': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
      default: return {0x1F, 0x11, 0x05, 0x02, 0x04, 0x00, 0x04};
    }
  }

  void draw_char(int x, int y, char c, int scale) {
    const auto rows = glyph_rows(c);
    for (int row = 0; row < 7; ++row) {
      for (int col = 0; col < 5; ++col) {
        if ((rows[static_cast<size_t>(row)] & (0x10 >> col)) != 0) {
          fill_rect(x + col * scale, y + row * scale, scale, scale, true);
        }
      }
    }
  }

  int width_ = 0;
  int height_ = 0;
  int bytes_per_row_ = 0;
  std::vector<uint8_t> buffer_;
};

std::string fit_text(const EpaperCanvas& canvas, const std::string& text, int scale,
                     int max_width) {
  if (max_width <= 0 || text.empty()) return {};
  if (canvas.text_width(text, scale) <= max_width) return text;
  const std::string suffix = canvas.text_width("...", scale) <= max_width ? "..." : "";
  std::string out = text;
  while (!out.empty() && canvas.text_width(out + suffix, scale) > max_width) out.pop_back();
  return out + suffix;
}

class EpaperDisplay {
 public:
  bool initialize() {
    if (initialized_) return true;
    pinMode(EPAPER_PIN_DC, OUTPUT);
    pinMode(EPAPER_PIN_RST, OUTPUT);
    pinMode(EPAPER_PIN_CS, OUTPUT);
    pinMode(EPAPER_PIN_BUSY, INPUT_PULLUP);
    digitalWrite(EPAPER_PIN_CS, HIGH);
    SPI.begin(EPAPER_PIN_SCLK, -1, EPAPER_PIN_MOSI, EPAPER_PIN_CS);
    SPI.beginTransaction(SPISettings(4000000, MSBFIRST, SPI_MODE0));
    if (!reset_panel()) return false;
    if (!configure_panel()) return false;
    initialized_ = true;
    Serial.printf("E-paper ready: %dx%d MOSI=%d CLK=%d CS=%d DC=%d RST=%d BUSY=%d idle=%d\n",
                  EPAPER_WIDTH, EPAPER_HEIGHT, EPAPER_PIN_MOSI, EPAPER_PIN_SCLK, EPAPER_PIN_CS,
                  EPAPER_PIN_DC, EPAPER_PIN_RST, EPAPER_PIN_BUSY, EPAPER_BUSY_IDLE_LEVEL);
    return true;
  }

  bool show_status(const EpaperStatusLines& lines) {
    if (!initialize()) return false;
    EpaperCanvas canvas(EPAPER_WIDTH, EPAPER_HEIGHT);
    render_status(&canvas, lines);
    const std::vector<uint8_t>& frame = canvas.buffer();
    const DirtyRect dirty = dirty_rect(frame);
    if (dirty.w == 0 || dirty.h == 0) return true;
    if (should_full_refresh(dirty)) {
      set_ram_window(0, (EPAPER_WIDTH + 7) / 8 - 1, 0, EPAPER_HEIGHT - 1);
      set_ram_pointer(0, 0);
      write_frame(frame);
      set_ram_pointer(0, 0);
      send_command(0x26);
      send_data(frame.data(), frame.size());
      refresh(kFullRefreshControl);
      partial_refresh_count_ = 0;
    } else {
      set_ram_window(0, (EPAPER_WIDTH + 7) / 8 - 1, 0, EPAPER_HEIGHT - 1);
      set_ram_pointer(0, 0);
      send_command(0x26);
      send_data(last_frame_.data(), last_frame_.size());
      set_ram_pointer(0, 0);
      write_frame(frame);
      refresh(kPartialRefreshControl);
      ++partial_refresh_count_;
    }
    last_frame_ = frame;
    has_last_frame_ = true;
    return true;
  }

 private:
  struct DirtyRect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
  };

  bool reset_panel() {
    digitalWrite(EPAPER_PIN_RST, HIGH);
    delay(20);
    digitalWrite(EPAPER_PIN_RST, LOW);
    delay(2);
    digitalWrite(EPAPER_PIN_RST, HIGH);
    delay(20);
    return wait_until_idle(kBusyTimeoutMs);
  }

  bool configure_panel() {
    send_command(0x12);
    if (!wait_until_idle(kBusyTimeoutMs)) return false;
    send_command(0x01);
    send_data(static_cast<uint8_t>((EPAPER_HEIGHT - 1) & 0xFF));
    send_data(static_cast<uint8_t>(((EPAPER_HEIGHT - 1) >> 8) & 0xFF));
    send_data(0x00);
    send_command(0x11);
    send_data(0x03);
    send_command(0x44);
    send_data(0x00);
    send_data(static_cast<uint8_t>((EPAPER_WIDTH + 7) / 8 - 1));
    send_command(0x45);
    send_data(0x00);
    send_data(0x00);
    send_data(static_cast<uint8_t>((EPAPER_HEIGHT - 1) & 0xFF));
    send_data(static_cast<uint8_t>(((EPAPER_HEIGHT - 1) >> 8) & 0xFF));
    send_command(0x3C);
    send_data(0x05);
    send_command(0x18);
    send_data(0x80);
    send_command(0x4E);
    send_data(0x00);
    send_command(0x4F);
    send_data(0x00);
    send_data(0x00);
    return wait_until_idle(kBusyTimeoutMs);
  }

  void set_ram_window(int x_byte_start, int x_byte_end, int y_start, int y_end) {
    x_byte_start = std::max(0, std::min(x_byte_start, (EPAPER_WIDTH + 7) / 8 - 1));
    x_byte_end = std::max(x_byte_start, std::min(x_byte_end, (EPAPER_WIDTH + 7) / 8 - 1));
    y_start = std::max(0, std::min(y_start, EPAPER_HEIGHT - 1));
    y_end = std::max(y_start, std::min(y_end, EPAPER_HEIGHT - 1));
    send_command(0x44);
    send_data(static_cast<uint8_t>(x_byte_start));
    send_data(static_cast<uint8_t>(x_byte_end));
    send_command(0x45);
    send_data(static_cast<uint8_t>(y_start & 0xFF));
    send_data(static_cast<uint8_t>((y_start >> 8) & 0xFF));
    send_data(static_cast<uint8_t>(y_end & 0xFF));
    send_data(static_cast<uint8_t>((y_end >> 8) & 0xFF));
  }

  void set_ram_pointer(int x_byte, int y) {
    x_byte = std::max(0, std::min(x_byte, (EPAPER_WIDTH + 7) / 8 - 1));
    y = std::max(0, std::min(y, EPAPER_HEIGHT - 1));
    send_command(0x4E);
    send_data(static_cast<uint8_t>(x_byte));
    send_command(0x4F);
    send_data(static_cast<uint8_t>(y & 0xFF));
    send_data(static_cast<uint8_t>((y >> 8) & 0xFF));
  }

  void write_frame(const std::vector<uint8_t>& frame) {
    send_command(0x24);
    send_data(frame.data(), frame.size());
  }

  void refresh(uint8_t update_control) {
    send_command(0x22);
    send_data(update_control);
    send_command(0x20);
    wait_until_idle(kBusyTimeoutMs);
  }

  bool wait_until_idle(uint32_t timeout_ms) {
    const uint32_t start = millis();
    while (digitalRead(EPAPER_PIN_BUSY) != EPAPER_BUSY_IDLE_LEVEL) {
      if (millis() - start > timeout_ms) {
        Serial.printf("E-paper busy timeout: level=%d idle=%d\n", digitalRead(EPAPER_PIN_BUSY),
                      EPAPER_BUSY_IDLE_LEVEL);
        return false;
      }
      delay(10);
    }
    return true;
  }

  void send_command(uint8_t command) {
    digitalWrite(EPAPER_PIN_DC, LOW);
    digitalWrite(EPAPER_PIN_CS, LOW);
    SPI.transfer(command);
    digitalWrite(EPAPER_PIN_CS, HIGH);
  }

  void send_data(uint8_t data) { send_data(&data, 1); }

  void send_data(const uint8_t* data, size_t len) {
    digitalWrite(EPAPER_PIN_DC, HIGH);
    digitalWrite(EPAPER_PIN_CS, LOW);
    for (size_t i = 0; i < len; ++i) SPI.transfer(data[i]);
    digitalWrite(EPAPER_PIN_CS, HIGH);
  }

  void render_status(EpaperCanvas* canvas, const EpaperStatusLines& lines) {
    canvas->clear();
    const int width = canvas->width();
    const int content_width = width - 12;
    canvas->fill_rect(0, 0, width, 20, true);
    canvas->draw_text(6, 4, fit_text(*canvas, "PRINTSPHERE", 1, content_width - 42), 1);
    canvas->draw_right_text(width - 6, 4, fit_text(*canvas, lines.progress, 2, 38), 2);
    canvas->draw_text(6, 34, fit_text(*canvas, lines.status, 2, content_width), 2);
    canvas->draw_text(6, 60, fit_text(*canvas, lines.detail, 1, content_width), 1);
    canvas->fill_rect(0, 86, width, 2, true);
    canvas->draw_text(6, 104, fit_text(*canvas, "LEFT", 1, 44), 1);
    canvas->draw_right_text(width - 6, 100, fit_text(*canvas, lines.remaining, 2, 84), 2);
    canvas->draw_text(6, 136, fit_text(*canvas, "LAYER", 1, 50), 1);
    canvas->draw_right_text(width - 6, 132, fit_text(*canvas, lines.layers, 2, 84), 2);
    canvas->draw_text(6, 172, fit_text(*canvas, lines.temperatures, 1, content_width), 1);
    canvas->fill_rect(0, 204, width, 1, true);
    canvas->draw_text(6, 220, fit_text(*canvas, lines.network, 1, content_width), 1);
    canvas->draw_text(6, 242, fit_text(*canvas, lines.clock, 1, content_width), 1);
  }

  DirtyRect dirty_rect(const std::vector<uint8_t>& next) const {
    if (!has_last_frame_ || last_frame_.size() != next.size()) {
      DirtyRect rect;
      rect.x = 0;
      rect.y = 0;
      rect.w = EPAPER_WIDTH;
      rect.h = EPAPER_HEIGHT;
      return rect;
    }
    const int bytes_per_row = (EPAPER_WIDTH + 7) / 8;
    int min_byte = bytes_per_row;
    int max_byte = -1;
    int min_y = EPAPER_HEIGHT;
    int max_y = -1;
    for (int y = 0; y < EPAPER_HEIGHT; ++y) {
      for (int xb = 0; xb < bytes_per_row; ++xb) {
        const size_t idx = static_cast<size_t>(y * bytes_per_row + xb);
        if (last_frame_[idx] != next[idx]) {
          min_byte = std::min(min_byte, xb);
          max_byte = std::max(max_byte, xb);
          min_y = std::min(min_y, y);
          max_y = std::max(max_y, y);
        }
      }
    }
    if (max_byte < min_byte || max_y < min_y) return {};
    const int x = min_byte * 8;
    const int w = std::min(EPAPER_WIDTH, (max_byte + 1) * 8) - x;
    DirtyRect rect;
    rect.x = x;
    rect.y = min_y;
    rect.w = w;
    rect.h = max_y - min_y + 1;
    return rect;
  }

  bool should_full_refresh(const DirtyRect& dirty) const {
    if (!has_last_frame_ || partial_refresh_count_ >= kMaxPartialBeforeFull) return true;
    return dirty.w * dirty.h >= kMaxPartialPixels;
  }

  bool initialized_ = false;
  std::vector<uint8_t> last_frame_;
  bool has_last_frame_ = false;
  uint8_t partial_refresh_count_ = 0;
};

EpaperDisplay gDisplay;

EpaperStatusLines build_epaper_status_lines(const PrinterSnapshot& snapshot) {
  EpaperStatusLines lines;
  if (snapshot.connection == PrinterConnectionState::kWaitingForCredentials) {
    lines.status = "SETUP";
  } else if (snapshot.connection == PrinterConnectionState::kError || snapshot.has_error ||
             snapshot.lifecycle == PrintLifecycleState::kError) {
    lines.status = "ERROR";
  } else if (!snapshot.wifi_connected && snapshot.connection != PrinterConnectionState::kBooting) {
    lines.status = "OFFLINE";
  } else if (!snapshot.raw_status.empty()) {
    lines.status = uppercase_ascii(snapshot.raw_status);
  } else {
    switch (snapshot.lifecycle) {
      case PrintLifecycleState::kPrinting: lines.status = "PRINTING"; break;
      case PrintLifecycleState::kPreparing: lines.status = "PREP"; break;
      case PrintLifecycleState::kPaused: lines.status = "PAUSED"; break;
      case PrintLifecycleState::kFinished: lines.status = "DONE"; break;
      case PrintLifecycleState::kIdle: lines.status = "IDLE"; break;
      case PrintLifecycleState::kError: lines.status = "ERROR"; break;
      default: lines.status = snapshot.connection == PrinterConnectionState::kBooting ? "BOOT" : "WAITING";
    }
  }
  if (snapshot.connection == PrinterConnectionState::kWaitingForCredentials) {
    lines.detail = "AP " + snapshot.setup_ap_ssid;
  } else if (!snapshot.job_name.empty() &&
             (snapshot.lifecycle == PrintLifecycleState::kPreparing ||
              snapshot.lifecycle == PrintLifecycleState::kPrinting ||
              snapshot.lifecycle == PrintLifecycleState::kPaused)) {
    lines.detail = snapshot.job_name;
  } else if (!snapshot.detail.empty()) {
    lines.detail = snapshot.detail;
  } else {
    lines.detail = "PrintSphere";
  }
  char buf[40] = {};
  std::snprintf(buf, sizeof(buf), "%d%%",
                std::max(0, std::min(100, static_cast<int>(snapshot.progress_percent + 0.5f))));
  lines.progress = buf;
  if (snapshot.lifecycle == PrintLifecycleState::kFinished) {
    lines.remaining = "done";
  } else if (snapshot.remaining_seconds == 0) {
    lines.remaining = "--";
  } else {
    const uint32_t total_minutes = snapshot.remaining_seconds / 60U;
    const uint32_t hours = total_minutes / 60U;
    const uint32_t minutes = total_minutes % 60U;
    if (hours > 0) std::snprintf(buf, sizeof(buf), "%luh %02lum",
                                 static_cast<unsigned long>(hours),
                                 static_cast<unsigned long>(minutes));
    else std::snprintf(buf, sizeof(buf), "%lum", static_cast<unsigned long>(minutes));
    lines.remaining = buf;
  }
  if (snapshot.nozzle_temp_known && snapshot.bed_temp_known) {
    std::snprintf(buf, sizeof(buf), "N %.0fC  B %.0fC", snapshot.nozzle_temp_c, snapshot.bed_temp_c);
  } else if (snapshot.nozzle_temp_known) {
    std::snprintf(buf, sizeof(buf), "N %.0fC  B --", snapshot.nozzle_temp_c);
  } else if (snapshot.bed_temp_known) {
    std::snprintf(buf, sizeof(buf), "N --  B %.0fC", snapshot.bed_temp_c);
  } else {
    std::snprintf(buf, sizeof(buf), "N --  B --");
  }
  lines.temperatures = buf;
  if (snapshot.total_layers > 0) {
    std::snprintf(buf, sizeof(buf), "L %u/%u", snapshot.current_layer, snapshot.total_layers);
  } else if (snapshot.current_layer > 0) {
    std::snprintf(buf, sizeof(buf), "L %u/--", snapshot.current_layer);
  } else {
    std::snprintf(buf, sizeof(buf), "L --/--");
  }
  lines.layers = buf;
  if (snapshot.connection == PrinterConnectionState::kWaitingForCredentials) {
    lines.network = "Open 192.168.4.1";
  } else if (snapshot.wifi_connected && !snapshot.wifi_ip.empty()) {
    lines.network = "WiFi " + snapshot.wifi_ip;
  } else {
    lines.network = snapshot.wifi_connected ? "WiFi connected" : "WiFi offline";
  }
  const time_t now = time(nullptr);
  if (now < 1704067200) {
    lines.clock = "TIME NOT SYNCED";
  } else {
    tm tm_now = {};
    localtime_r(&now, &tm_now);
    std::strftime(buf, sizeof(buf), "TIME %Y-%m-%d %H:%M", &tm_now);
    lines.clock = buf;
  }
  return lines;
}

uint32_t hash_mix(uint32_t hash, const std::string& text) {
  for (unsigned char c : text) {
    hash ^= c;
    hash *= 16777619U;
  }
  hash ^= 0x9E3779B9U;
  return hash;
}

uint32_t status_signature(const EpaperStatusLines& lines) {
  uint32_t hash = 2166136261U;
  hash = hash_mix(hash, lines.status);
  hash = hash_mix(hash, lines.detail);
  hash = hash_mix(hash, lines.progress);
  hash = hash_mix(hash, lines.remaining);
  hash = hash_mix(hash, lines.temperatures);
  hash = hash_mix(hash, lines.layers);
  hash = hash_mix(hash, lines.network);
  hash = hash_mix(hash, lines.clock);
  return hash;
}

class BambuCloudClient {
 public:
  void begin() {
    tls_.setCACert(kCloudCaBundle);
    mqtt_.setClient(tls_);
    mqtt_.setBufferSize(kMqttBufferBytes);
    mqtt_.setKeepAlive(30);
    mqtt_.setSocketTimeout(10);
    mqtt_.setCallback([this](char* topic, uint8_t* payload, unsigned int length) {
      handle_mqtt_message(topic, payload, length);
    });
    load_from_store();
  }

  void load_from_store() {
    credentials_ = gConfig.load_cloud();
    printer_ = gConfig.load_printer();
    access_token_ = gConfig.load_token();
    mqtt_username_.clear();
    mqtt_report_topic_.clear();
    mqtt_request_topic_.clear();
    connected_ = false;
    mutate_snapshot([&](PrinterSnapshot& s) {
      s.cloud_configured = credentials_.can_password_login() || !access_token_.empty();
      s.resolved_serial = printer_.serial;
      if (!s.cloud_configured) {
        s.connection = WiFi.isConnected() ? PrinterConnectionState::kWaitingForCredentials
                                          : PrinterConnectionState::kConnecting;
        s.detail = "Cloud login not configured";
      }
    });
  }

  void submit_verification_code(const std::string& code, bool tfa = false) {
    pending_code_ = code;
    auth_mode_ = tfa ? AuthMode::kTfaCode : AuthMode::kEmailCode;
    next_auth_ms_ = 0;
  }

  void loop() {
    if (!WiFi.isConnected()) {
      if (mqtt_.connected()) mqtt_.disconnect();
      connected_ = false;
      mutate_snapshot([](PrinterSnapshot& s) {
        s.cloud_connected = false;
        s.wifi_connected = false;
        s.connection = PrinterConnectionState::kConnecting;
        s.detail = "Waiting for Wi-Fi for Bambu Cloud";
      });
      delay(1000);
      return;
    }

    mutate_snapshot([](PrinterSnapshot& s) {
      s.wifi_connected = true;
      s.wifi_ip = to_std(WiFi.localIP().toString());
    });

    if (access_token_.empty()) {
      if (!credentials_.can_password_login() || millis() < next_auth_ms_) {
        delay(1000);
        return;
      }
      if (waiting_for_user_code()) {
        delay(1000);
        return;
      }
      authenticate();
      delay(1000);
      return;
    }

    if (printer_.serial.empty()) fetch_bindings();
    if (!ensure_mqtt_identity() || printer_.serial.empty()) {
      delay(2000);
      return;
    }

    if (!mqtt_.connected()) connect_mqtt();
    mqtt_.loop();
    if (mqtt_.connected() && millis() - last_sync_ms_ > 30000) {
      publish(kGetVersion);
      publish(kPushAll);
      last_sync_ms_ = millis();
    }
    delay(20);
  }

  bool cloud_ready() const { return connected_; }
  std::string detail() const { return detail_; }

 private:
  enum class AuthMode : uint8_t { kPassword, kEmailCode, kTfaCode };

  bool waiting_for_user_code() const {
    return auth_mode_ != AuthMode::kPassword && pending_code_.empty();
  }

  const char* api_base() const {
    return credentials_.region == CloudRegion::kCN ? "https://api.bambulab.cn"
                                                   : "https://api.bambulab.com";
  }

  const char* mqtt_host() const {
    return credentials_.region == CloudRegion::kCN ? "cn.mqtt.bambulab.com"
                                                   : "us.mqtt.bambulab.com";
  }

  std::string api_url(const char* path) const { return std::string(api_base()) + path; }

  bool perform_json_request(const std::string& url, const char* method,
                            const std::string& request_body, const std::string& bearer,
                            int* status_code, std::string* response_body) {
    if (status_code == nullptr || response_body == nullptr) return false;
    response_body->clear();
    *status_code = 0;

    start_time_sync();
    WiFiClientSecure client;
    client.setCACert(kCloudCaBundle);
    HTTPClient http;
    http.setTimeout(10000);
    if (!http.begin(client, url.c_str())) return false;
    http.setReuse(false);
    http.setUserAgent("bambu_network_agent/01.09.05.01");
    http.addHeader("X-BBL-Client-Name", "OrcaSlicer");
    http.addHeader("X-BBL-Client-Type", "slicer");
    http.addHeader("X-BBL-Client-Version", "01.09.05.51");
    http.addHeader("X-BBL-Language", "en-US");
    http.addHeader("X-BBL-OS-Type", "linux");
    http.addHeader("X-BBL-OS-Version", "6.2.0");
    http.addHeader("X-BBL-Agent-Version", "01.09.05.01");
    http.addHeader("X-BBL-Executable-info", "{}");
    http.addHeader("X-BBL-Agent-OS-Type", "linux");
    http.addHeader("Accept", "application/json");
    http.addHeader("Accept-Encoding", "identity");
    if (!request_body.empty()) http.addHeader("Content-Type", "application/json");
    if (!bearer.empty()) http.addHeader("Authorization", ("Bearer " + bearer).c_str());

    *status_code = strcmp(method, "POST") == 0 ? http.POST(request_body.c_str()) : http.GET();
    if (*status_code <= 0) {
      Serial.printf("HTTP %s failed for %s: %s\n", method, url.c_str(),
                    http.errorToString(*status_code).c_str());
      http.end();
      return false;
    }
    *response_body = to_std(http.getString());
    http.end();
    return true;
  }

  bool authenticate() {
    mutate_snapshot([](PrinterSnapshot& s) {
      s.cloud_configured = true;
      s.connection = PrinterConnectionState::kConnecting;
      s.detail = "Logging in to Bambu Cloud";
    });

    JsonDocument body;
    if (auth_mode_ == AuthMode::kEmailCode) {
      body["account"] = credentials_.email;
      body["code"] = pending_code_;
    } else {
      body["account"] = credentials_.email;
      body["password"] = credentials_.password;
      body["apiError"] = "";
    }
    String request;
    serializeJson(body, request);

    int status = 0;
    std::string response;
    if (!perform_json_request(api_url(kLoginPath), "POST", to_std(request), {}, &status, &response)) {
      set_detail("Bambu Cloud login request failed; check region/network", true);
      next_auth_ms_ = millis() + 30000;
      return false;
    }

    JsonDocument root;
    DeserializationError err = deserializeJson(root, response);
    if (err) {
      set_detail("Bambu Cloud login invalid JSON HTTP " + std::to_string(status) + ": " +
                     compact_response_preview(response),
                 true);
      next_auth_ms_ = millis() + 30000;
      return false;
    }

    JsonVariantConst data = root["data"];
    std::string token = json_string(root.as<JsonVariantConst>(), "accessToken",
                                    json_string(data, "accessToken", {}));
    std::string login_type = json_string(root.as<JsonVariantConst>(), "loginType",
                                         json_string(data, "loginType", {}));
    std::string api_error = json_string(root.as<JsonVariantConst>(), "apiError",
                                        json_string(data, "apiError",
                                                    json_string(root.as<JsonVariantConst>(), "msg", {})));

    if (login_type == "verifyCode") {
      auth_mode_ = AuthMode::kEmailCode;
      request_verification_code();
      set_detail(credentials_.email.find('@') == std::string::npos
                     ? "Bambu Cloud sent SMS code; enter it in setup portal"
                     : "Bambu Cloud sent email code; enter it in setup portal",
                 false);
      return false;
    }
    if (login_type == "tfa") {
      auth_mode_ = AuthMode::kTfaCode;
      set_detail("Bambu Cloud requires 2FA code", false);
      return false;
    }
    if (status < 200 || status >= 300 || token.empty()) {
      if (auth_mode_ == AuthMode::kEmailCode) {
        const int code = root["code"] | data["code"] | -1;
        pending_code_.clear();
        if (status == 400 && code == 1) {
          request_verification_code();
          set_detail("Bambu Cloud verification code expired; new code requested", false);
          return false;
        }
        if (status == 400 && code == 2) {
          set_detail("Bambu Cloud verification code incorrect", false);
          return false;
        }
        set_detail("Bambu Cloud verification-code login rejected", true);
        next_auth_ms_ = millis() + 30000;
        return false;
      }
      set_detail(!api_error.empty() ? "Bambu Cloud login failed: " + api_error
                                    : "Bambu Cloud login rejected (HTTP " + std::to_string(status) + ")",
                 true);
      next_auth_ms_ = millis() + 30000;
      return false;
    }

    access_token_ = token;
    gConfig.save_token(access_token_);
    credentials_.password.clear();
    gConfig.save_cloud(credentials_);
    auth_mode_ = AuthMode::kPassword;
    pending_code_.clear();
    mqtt_username_.clear();
    set_detail("Bambu Cloud session ready", false);
    fetch_bindings();
    return true;
  }

  bool request_verification_code() {
    JsonDocument body;
    const bool is_phone = credentials_.email.find('@') == std::string::npos;
    body[is_phone ? "phone" : "email"] = credentials_.email;
    body["type"] = "codeLogin";
    String request;
    serializeJson(body, request);
    int status = 0;
    std::string response;
    const bool ok = perform_json_request(api_url(is_phone ? kSmsCodePath : kEmailCodePath), "POST",
                                         to_std(request), {}, &status, &response);
    return ok && status >= 200 && status < 300;
  }

  bool fetch_bindings() {
    int status = 0;
    std::string response;
    if (!perform_json_request(api_url(kBindPath), "GET", {}, access_token_, &status, &response)) {
      return false;
    }
    if (status == 401 || status == 403) {
      access_token_.clear();
      gConfig.clear_token();
      set_detail("Bambu Cloud token expired; login again", true);
      return false;
    }
    if (status < 200 || status >= 300) return false;

    JsonDocument root;
    if (deserializeJson(root, response)) return false;
    JsonArrayConst devices;
    if (root["devices"].is<JsonArrayConst>()) devices = root["devices"].as<JsonArrayConst>();
    else if (root["data"]["devices"].is<JsonArrayConst>()) devices = root["data"]["devices"].as<JsonArrayConst>();
    if (!devices) return false;

    std::string best_serial;
    for (JsonVariantConst item : devices) {
      const std::string candidate = json_string(item, "dev_id",
          json_string(item, "serial", json_string(item, "dev_id_str",
          json_string(item, "device_sn", json_string(item, "dev_sn", {})))));
      if (candidate.empty()) continue;
      if (printer_.serial.empty() || candidate == printer_.serial) {
        best_serial = candidate;
        break;
      }
    }
    if (!best_serial.empty()) {
      printer_.serial = best_serial;
      gConfig.save_printer(printer_);
      mutate_snapshot([&](PrinterSnapshot& s) {
        s.resolved_serial = best_serial;
        s.detail = "Bambu Cloud session ready";
      });
    }
    return true;
  }

  bool ensure_mqtt_identity() {
    if (!mqtt_username_.empty()) return true;
    mqtt_username_ = decode_username_from_access_token(access_token_);
    if (!mqtt_username_.empty()) return true;

    int status = 0;
    std::string response;
    if (!perform_json_request(api_url(kPreferencePath), "GET", {}, access_token_, &status,
                              &response)) {
      return false;
    }
    if (status < 200 || status >= 300) return false;
    JsonDocument root;
    if (deserializeJson(root, response)) return false;
    int64_t uid = -1;
    if (root["uid"].is<int64_t>()) uid = root["uid"].as<int64_t>();
    else if (root["data"]["uid"].is<int64_t>()) uid = root["data"]["uid"].as<int64_t>();
    else if (root["uidStr"].is<const char*>()) uid = std::strtoll(root["uidStr"], nullptr, 10);
    else if (root["data"]["uidStr"].is<const char*>()) uid = std::strtoll(root["data"]["uidStr"], nullptr, 10);
    if (uid > 0) mqtt_username_ = "u_" + std::to_string(uid);
    return !mqtt_username_.empty();
  }

  std::string decode_username_from_access_token(const std::string& token) {
    const size_t first_dot = token.find('.');
    if (first_dot == std::string::npos) return {};
    const size_t second_dot = token.find('.', first_dot + 1);
    if (second_dot == std::string::npos || second_dot <= first_dot + 1) return {};
    std::string payload_b64 = token.substr(first_dot + 1, second_dot - first_dot - 1);
    std::replace(payload_b64.begin(), payload_b64.end(), '-', '+');
    std::replace(payload_b64.begin(), payload_b64.end(), '_', '/');
    while ((payload_b64.size() % 4U) != 0U) payload_b64.push_back('=');
    std::vector<unsigned char> decoded(payload_b64.size());
    size_t decoded_len = 0;
    const int result = mbedtls_base64_decode(
        decoded.data(), decoded.size(), &decoded_len,
        reinterpret_cast<const unsigned char*>(payload_b64.data()), payload_b64.size());
    if (result != 0 || decoded_len == 0) return {};
    JsonDocument root;
    if (deserializeJson(root, decoded.data(), decoded_len)) return {};
    std::string username = json_string(root.as<JsonVariantConst>(), "username", {});
    if (!username.empty()) return username;
    int64_t uid = -1;
    if (root["uid"].is<int64_t>()) uid = root["uid"].as<int64_t>();
    else if (root["uid"].is<const char*>()) uid = std::strtoll(root["uid"], nullptr, 10);
    return uid > 0 ? "u_" + std::to_string(uid) : std::string{};
  }

  bool connect_mqtt() {
    if (millis() < next_mqtt_ms_) return false;
    const std::string serial = printer_.serial;
    mqtt_report_topic_ = "device/" + serial + "/report";
    mqtt_request_topic_ = "device/" + serial + "/request";
    mqtt_.setServer(mqtt_host(), kCloudMqttPort);
    const std::string client_id = "printsphere-cloud-" + std::to_string(static_cast<unsigned>(esp_random()));
    Serial.printf("Connecting to Bambu Cloud MQTT %s:%u serial=%s user=%s\n", mqtt_host(),
                  kCloudMqttPort, serial.c_str(), mqtt_username_.c_str());
    if (!mqtt_.connect(client_id.c_str(), mqtt_username_.c_str(), access_token_.c_str())) {
      Serial.printf("Cloud MQTT connect failed state=%d\n", mqtt_.state());
      connected_ = false;
      next_mqtt_ms_ = millis() + 10000;
      mutate_snapshot([](PrinterSnapshot& s) {
        s.cloud_connected = false;
        s.connection = PrinterConnectionState::kConnecting;
        s.detail = "Connecting to Bambu Cloud MQTT";
      });
      return false;
    }
    connected_ = true;
    mqtt_.subscribe(mqtt_report_topic_.c_str(), 1);
    mutate_snapshot([](PrinterSnapshot& s) {
      s.cloud_connected = true;
      s.connection = PrinterConnectionState::kOnline;
      s.detail = "Cloud MQTT subscribed, requesting printer sync";
    });
    publish(kGetVersion);
    publish(kPushAll);
    last_sync_ms_ = millis();
    return true;
  }

  bool publish(const char* payload) {
    if (!mqtt_.connected() || mqtt_request_topic_.empty()) return false;
    return mqtt_.publish(mqtt_request_topic_.c_str(), payload);
  }

  void handle_mqtt_message(char* topic, uint8_t* payload, unsigned int length) {
    if (topic == nullptr || mqtt_report_topic_ != topic) return;
    std::vector<uint8_t> copy(payload, payload + length);
    copy.push_back('\0');
    JsonDocument root;
    DeserializationError err = deserializeJson(root, copy.data(), length);
    if (err) {
      Serial.printf("Cloud MQTT payload JSON parse failed: %s\n", err.c_str());
      return;
    }
    JsonVariantConst print = root["print"];
    if (!print.is<JsonObjectConst>() && root["pushall"]["print"].is<JsonObjectConst>()) {
      print = root["pushall"]["print"];
    }
    if (!print.is<JsonObjectConst>()) return;
    PrinterSnapshot current = read_snapshot();
    current.connection = PrinterConnectionState::kOnline;
    current.cloud_connected = true;
    current.cloud_configured = true;
    current.resolved_serial = printer_.serial;

    std::string status_text = first_string(print, {"gcode_state", "status", "task_status",
                                                   "taskStatus", "print_status", "printStatus",
                                                   "state"});
    JsonVariantConst nested_print = print["print"];
    if (status_text.empty() && nested_print.is<JsonObjectConst>()) {
      status_text = first_string(nested_print, {"gcode_state", "status", "task_status",
                                                "taskStatus", "print_status", "printStatus",
                                                "state"});
    }
    std::string stage_text = first_string(print, {"current_stage", "currentStage", "stage_name",
                                                  "stageName", "stage"});
    if (stage_text.empty() && nested_print.is<JsonObjectConst>()) {
      stage_text = first_string(nested_print, {"current_stage", "currentStage", "stage_name",
                                               "stageName", "stage"});
    }
    if (!status_text.empty()) {
      current.raw_status = status_text;
      current.lifecycle = lifecycle_from_bambu_status(status_text);
      current.stage = !stage_text.empty() ? stage_text : default_stage_label_for_status(status_text);
    }
    if (!stage_text.empty()) {
      current.raw_stage = stage_text;
      current.stage = stage_text;
    }

    const std::string job = trim_job_name(first_string(print, {"subtask_name", "gcode_file"}));
    if (!job.empty()) current.job_name = job;

    ProgressValue progress = extract_progress(print, current.lifecycle == PrintLifecycleState::kPreparing);
    if (current.lifecycle == PrintLifecycleState::kFinished && (!progress.present || progress.value < 100.0f)) {
      current.progress_percent = 100.0f;
    } else if (progress.present || current.lifecycle == PrintLifecycleState::kIdle ||
               current.lifecycle == PrintLifecycleState::kError) {
      current.progress_percent = progress.value;
    }

    current.remaining_seconds = extract_remaining_seconds(print, current.remaining_seconds);
    current.current_layer = static_cast<uint16_t>(std::max(0, extract_int_any(print, {"layer_num", "current_layer", "currentLayer", "layer"}, current.current_layer)));
    current.total_layers = static_cast<uint16_t>(std::max(0, extract_int_any(print, {"total_layer_num", "total_layers", "totalLayers", "layer_count", "layerCount"}, current.total_layers)));
    const float nozzle = extract_float_any(print, {"nozzle_temper", "nozzle_temp", "nozzle_temperature", "hotend_temp"}, NAN);
    if (!std::isnan(nozzle)) {
      current.nozzle_temp_c = normalize_temperature(nozzle);
      current.nozzle_temp_known = true;
    }
    const float bed = extract_float_any(print, {"bed_temper", "bed_temp", "bed_temperature", "hotbed_temp"}, NAN);
    if (!std::isnan(bed)) {
      current.bed_temp_c = normalize_temperature(bed);
      current.bed_temp_known = true;
    }

    current.has_error = current.lifecycle == PrintLifecycleState::kError;
    current.detail = current.has_error ? "Print error" : (!current.stage.empty() ? current.stage : "Connected to Bambu Cloud");
    mutate_snapshot([&](PrinterSnapshot& s) { s = current; });
  }

  struct ProgressValue {
    bool present = false;
    float value = 0.0f;
  };

  std::string first_string(JsonVariantConst object, std::initializer_list<const char*> keys) {
    for (const char* key : keys) {
      std::string value = json_string(object, key, {});
      if (!value.empty()) return value;
    }
    return {};
  }

  int extract_int_any(JsonVariantConst object, std::initializer_list<const char*> keys, int fallback) {
    for (const char* key : keys) {
      JsonVariantConst item = object[key];
      if (item.is<int>()) return item.as<int>();
      if (item.is<const char*>()) return std::strtol(item.as<const char*>(), nullptr, 0);
    }
    JsonVariantConst nested = object["print"];
    if (nested.is<JsonObjectConst>()) {
      for (const char* key : keys) {
        JsonVariantConst item = nested[key];
        if (item.is<int>()) return item.as<int>();
        if (item.is<const char*>()) return std::strtol(item.as<const char*>(), nullptr, 0);
      }
    }
    return fallback;
  }

  float extract_float_any(JsonVariantConst object, std::initializer_list<const char*> keys, float fallback) {
    for (const char* key : keys) {
      JsonVariantConst item = object[key];
      if (item.is<float>() || item.is<double>() || item.is<int>()) return item.as<float>();
      if (item.is<const char*>()) return std::strtof(item.as<const char*>(), nullptr);
    }
    JsonVariantConst nested = object["print"];
    if (nested.is<JsonObjectConst>()) {
      for (const char* key : keys) {
        JsonVariantConst item = nested[key];
        if (item.is<float>() || item.is<double>() || item.is<int>()) return item.as<float>();
        if (item.is<const char*>()) return std::strtof(item.as<const char*>(), nullptr);
      }
    }
    return fallback;
  }

  ProgressValue extract_progress(JsonVariantConst object, bool prefer_download) {
    const char* generic[] = {"progress", "percent", "mc_percent", "task_progress",
                             "taskProgress", "print_progress", "printPercent"};
    const char* download[] = {"gcode_file_prepare_percent", "gcodeFilePreparePercent",
                              "prepare_percent", "preparePercent", "gcode_prepare_percent",
                              "gcodePreparePercent", "download_progress", "downloadProgress",
                              "model_download_progress", "modelDownloadProgress"};
    const auto read = [&](const char* const* keys, size_t count, float* value) {
      for (size_t i = 0; i < count; ++i) {
        const float v = extract_float_any(object, {keys[i]}, -1.0f);
        if (v >= 0.0f) {
          *value = v <= 1.0f ? v * 100.0f : v;
          return true;
        }
      }
      return false;
    };
    ProgressValue out;
    float candidate = 0.0f;
    if (prefer_download && read(download, sizeof(download) / sizeof(download[0]), &candidate)) {
      out.present = true;
      out.value = candidate;
      return out;
    }
    if (read(generic, sizeof(generic) / sizeof(generic[0]), &candidate) ||
        read(download, sizeof(download) / sizeof(download[0]), &candidate)) {
      out.present = true;
      out.value = candidate;
    }
    return out;
  }

  uint32_t extract_remaining_seconds(JsonVariantConst object, uint32_t fallback) {
    const int minutes = extract_int_any(object, {"mc_remaining_time", "remaining_minutes", "remainingMinutes"}, -1);
    if (minutes >= 0) return static_cast<uint32_t>(minutes) * 60U;
    const int seconds = extract_int_any(object, {"remaining_seconds", "remainingSeconds",
                                                 "remaining_time", "remainingTime", "mc_left_time"}, -1);
    return seconds >= 0 ? static_cast<uint32_t>(seconds) : fallback;
  }

  float normalize_temperature(float value) {
    if (value > 65535.0f) return static_cast<float>(static_cast<int>(value) & 0xFFFF);
    return value;
  }

  std::string trim_job_name(const std::string& name) {
    if (name.empty()) return {};
    std::string trimmed = name;
    const size_t slash = trimmed.find_last_of("/\\");
    if (slash != std::string::npos) trimmed = trimmed.substr(slash + 1);
    for (const char* suffix : {".gcode.3mf", ".3mf", ".gcode"}) {
      const size_t suffix_len = strlen(suffix);
      if (trimmed.size() >= suffix_len &&
          trimmed.compare(trimmed.size() - suffix_len, suffix_len, suffix) == 0) {
        trimmed.resize(trimmed.size() - suffix_len);
        break;
      }
    }
    return trimmed;
  }

  void set_detail(const std::string& detail, bool error) {
    detail_ = detail;
    mutate_snapshot([&](PrinterSnapshot& s) {
      s.cloud_configured = credentials_.can_password_login() || !access_token_.empty();
      s.cloud_connected = false;
      s.connection = error ? PrinterConnectionState::kError : PrinterConnectionState::kConnecting;
      s.detail = detail;
      s.has_error = error;
    });
  }

  WiFiClientSecure tls_;
  PubSubClient mqtt_;
  BambuCloudCredentials credentials_;
  PrinterConnection printer_;
  std::string access_token_;
  std::string mqtt_username_;
  std::string mqtt_report_topic_;
  std::string mqtt_request_topic_;
  std::string pending_code_;
  std::string detail_ = "Cloud login not configured";
  AuthMode auth_mode_ = AuthMode::kPassword;
  uint32_t next_auth_ms_ = 0;
  uint32_t next_mqtt_ms_ = 0;
  uint32_t last_sync_ms_ = 0;
  bool connected_ = false;
};

BambuCloudClient gCloud;
WebServer gServer(80);

String json_escape(const std::string& input) {
  String out;
  out.reserve(input.size() + 8);
  for (char c : input) {
    switch (c) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out += c; break;
    }
  }
  return out;
}

String portal_html() {
  return F(
      "<!doctype html><html><head><meta charset=\"utf-8\"><meta name=\"viewport\" "
      "content=\"width=device-width,initial-scale=1\"><title>PrintSphere Web Config</title>"
      "<style>:root{--bg:#0b1015;--panel:#121a23;--line:#263548;--text:#eef4fb;"
      "--muted:#9fb0c4;--accent:#f0a64b;--ok:#4ade80;--warn:#facc15;--bad:#fb7185}"
      "*{box-sizing:border-box}body{margin:0;background:#0b1015;color:var(--text);"
      "font-family:Segoe UI,system-ui,sans-serif}.page{width:min(900px,calc(100% - 28px));"
      "margin:0 auto;padding:28px 0 44px}.hero{display:flex;justify-content:space-between;"
      "gap:18px;align-items:flex-start;margin-bottom:18px}h1{margin:0;font-size:34px}"
      "p{color:var(--muted);line-height:1.45}.grid{display:grid;gap:14px}.card{border:1px solid "
      "var(--line);background:var(--panel);border-radius:8px;padding:18px}.row{display:grid;"
      "grid-template-columns:1fr 1fr;gap:12px}@media(max-width:680px){.row{grid-template-columns:1fr}}"
      "label{display:block;margin:10px 0 6px;color:#cbd5e1;font-weight:700}input,select{width:100%;"
      "border:1px solid #38506b;border-radius:6px;background:#0f1722;color:var(--text);padding:12px}"
      "button{border:0;border-radius:999px;padding:12px 18px;background:var(--accent);font-weight:800;"
      "cursor:pointer}.muted{color:var(--muted)}.status{font-weight:800}.ok{color:var(--ok)}"
      ".warn{color:var(--warn)}.bad{color:var(--bad)}</style></head><body><main class=\"page\">"
      "<div class=\"hero\"><div><h1>PrintSphere Web Config</h1><p>Configure Wi-Fi, Bambu Cloud, "
      "and the active printer serial.</p></div><div id=\"badge\" class=\"status warn\">Loading</div></div>"
      "<div class=\"grid\"><section class=\"card\"><h2>Wi-Fi</h2><div class=\"row\"><div><label>SSID</label>"
      "<input id=\"wifi_ssid\"></div><div><label>Password</label><input id=\"wifi_pass\" type=\"password\" "
      "placeholder=\"Leave empty to keep saved password\"></div></div></section>"
      "<section class=\"card\"><h2>Bambu Cloud</h2><div class=\"row\"><div><label>Region</label><select "
      "id=\"cloud_region\"><option value=\"cn\">China</option><option value=\"us\">US</option><option "
      "value=\"eu\">EU</option></select></div><div><label>Account</label><input id=\"cloud_email\"></div>"
      "</div><label>Password</label><input id=\"cloud_pass\" type=\"password\" placeholder=\"Leave empty "
      "to keep saved password\"><div style=\"margin-top:12px\"><button onclick=\"cloudConnect()\">Connect "
      "Cloud</button></div><p id=\"cloud_detail\" class=\"muted\"></p><div class=\"row\"><div><label>"
      "Verification code</label><input id=\"verify_code\" inputmode=\"numeric\"></div><div style=\"display:flex;"
      "align-items:end\"><button onclick=\"cloudVerify()\">Submit code</button></div></div></section>"
      "<section class=\"card\"><h2>Printer</h2><label>Serial</label><input id=\"printer_serial\"></section>"
      "<section class=\"card\"><button onclick=\"saveAll()\">Save config and reboot</button><p id=\"result\" "
      "class=\"muted\"></p></section></div></main><script>"
      "async function j(u,o){let r=await fetch(u,o);let t=await r.text();try{return JSON.parse(t)}catch(e){return {raw:t}}}"
      "async function load(){let c=await j('/api/config');wifi_ssid.value=c.wifi_ssid||'';cloud_email.value=c.cloud_email||'';"
      "cloud_region.value=c.cloud_region||'cn';printer_serial.value=c.printer_serial||'';tick()}"
      "async function tick(){let h=await j('/api/health');badge.textContent=h.cloud_connected?'Connected':(h.wifi_connected?'Wi-Fi ready':'Setup');"
      "badge.className='status '+(h.cloud_connected?'ok':(h.wifi_connected?'warn':'bad'));cloud_detail.textContent=h.detail||''}"
      "async function saveAll(){let body={wifi_ssid:wifi_ssid.value,wifi_pass:wifi_pass.value,cloud_email:cloud_email.value,"
      "cloud_pass:cloud_pass.value,cloud_region:cloud_region.value,printer_serial:printer_serial.value};let r=await j('/api/config',"
      "{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});result.textContent=r.detail||r.status||'saved'}"
      "async function cloudConnect(){let body={cloud_email:cloud_email.value,cloud_pass:cloud_pass.value,cloud_region:cloud_region.value,"
      "printer_serial:printer_serial.value};let r=await j('/api/cloud/connect',{method:'POST',headers:{'Content-Type':'application/json'},"
      "body:JSON.stringify(body)});result.textContent=r.detail||r.status||'saved'}"
      "async function cloudVerify(){let r=await j('/api/cloud/verify',{method:'POST',headers:{'Content-Type':'application/json'},"
      "body:JSON.stringify({code:verify_code.value})});result.textContent=r.detail||r.status||'submitted'}"
      "load();setInterval(tick,2000);</script></body></html>");
}

void send_json(int code, const String& body) {
  gServer.sendHeader("Cache-Control", "no-store");
  gServer.send(code, "application/json", body);
}

void handle_root() { gServer.send(200, "text/html", portal_html()); }

void handle_health() {
  PrinterSnapshot s = read_snapshot();
  String body = "{";
  body += "\"wifi_connected\":";
  body += WiFi.isConnected() ? "true" : "false";
  body += ",\"wifi_ip\":\"" + WiFi.localIP().toString() + "\"";
  body += ",\"setup_ap_ssid\":\"" + String(kDeviceName) + "-Setup\"";
  body += ",\"setup_ap_ip\":\"192.168.4.1\"";
  body += ",\"cloud_connected\":";
  body += s.cloud_connected ? "true" : "false";
  body += ",\"detail\":\"" + json_escape(s.detail) + "\"";
  body += "}";
  send_json(200, body);
}

void handle_config_get() {
  WifiCredentials wifi = gConfig.load_wifi();
  BambuCloudCredentials cloud = gConfig.load_cloud();
  PrinterConnection printer = gConfig.load_printer();
  String body = "{";
  body += "\"wifi_ssid\":\"" + json_escape(wifi.ssid) + "\"";
  body += ",\"wifi_password_saved\":";
  body += wifi.password.empty() ? "false" : "true";
  body += ",\"cloud_email\":\"" + json_escape(cloud.email) + "\"";
  body += ",\"cloud_password_saved\":";
  body += cloud.password.empty() ? "false" : "true";
  body += ",\"cloud_region\":\"" + to_string(cloud.region) + "\"";
  body += ",\"printer_serial\":\"" + json_escape(printer.serial) + "\"";
  body += "}";
  send_json(200, body);
}

bool read_body(JsonDocument& doc) {
  if (!gServer.hasArg("plain")) return false;
  return !deserializeJson(doc, gServer.arg("plain"));
}

void save_config_from_json(JsonDocument& doc, bool keep_blank_passwords) {
  WifiCredentials old_wifi = gConfig.load_wifi();
  BambuCloudCredentials old_cloud = gConfig.load_cloud();
  WifiCredentials wifi;
  wifi.ssid = json_string(doc.as<JsonVariantConst>(), "wifi_ssid", old_wifi.ssid);
  wifi.password = json_string(doc.as<JsonVariantConst>(), "wifi_pass", {});
  if (keep_blank_passwords && wifi.password.empty() && wifi.ssid == old_wifi.ssid) {
    wifi.password = old_wifi.password;
  }
  BambuCloudCredentials cloud;
  cloud.email = json_string(doc.as<JsonVariantConst>(), "cloud_email", old_cloud.email);
  cloud.password = json_string(doc.as<JsonVariantConst>(), "cloud_pass", {});
  if (keep_blank_passwords && cloud.password.empty() && cloud.email == old_cloud.email) {
    cloud.password = old_cloud.password;
  }
  cloud.region = parse_region(String(json_string(doc.as<JsonVariantConst>(), "cloud_region",
                                                 to_std(to_string(old_cloud.region))).c_str()));
  PrinterConnection printer;
  printer.serial = json_string(doc.as<JsonVariantConst>(), "printer_serial", gConfig.load_printer().serial);
  gConfig.save_wifi(wifi);
  gConfig.save_cloud(cloud);
  gConfig.save_printer(printer);
}

void reboot_later_task(void*) {
  delay(800);
  ESP.restart();
}

void handle_config_post() {
  JsonDocument doc;
  if (!read_body(doc)) {
    send_json(400, "{\"error\":\"invalid JSON\"}");
    return;
  }
  save_config_from_json(doc, true);
  send_json(200, "{\"status\":\"saved\",\"rebooting\":true,\"detail\":\"Saved. Rebooting.\"}");
  xTaskCreate(reboot_later_task, "portal_reboot", 2048, nullptr, 1, nullptr);
}

void handle_cloud_connect() {
  JsonDocument doc;
  if (!read_body(doc)) {
    send_json(400, "{\"error\":\"invalid JSON\"}");
    return;
  }
  save_config_from_json(doc, true);
  gCloud.load_from_store();
  send_json(200, "{\"status\":\"saved\",\"detail\":\"Cloud credentials saved. Login task will connect now.\"}");
}

void handle_cloud_verify() {
  JsonDocument doc;
  if (!read_body(doc)) {
    send_json(400, "{\"error\":\"invalid JSON\"}");
    return;
  }
  const std::string code = json_string(doc.as<JsonVariantConst>(), "code", {});
  if (code.empty()) {
    send_json(400, "{\"error\":\"verification code missing\"}");
    return;
  }
  gCloud.submit_verification_code(code);
  send_json(200, "{\"status\":\"submitted\",\"detail\":\"Verification code submitted.\"}");
}

void handle_wifi_scan() {
  int count = WiFi.scanNetworks();
  String body = "{\"networks\":[";
  for (int i = 0; i < count; ++i) {
    if (i > 0) body += ",";
    body += "{\"ssid\":\"" + json_escape(to_std(WiFi.SSID(i))) + "\",\"rssi\":";
    body += String(WiFi.RSSI(i));
    body += "}";
  }
  body += "]}";
  WiFi.scanDelete();
  send_json(200, body);
}

void portal_task(void*) {
  gServer.on("/", HTTP_GET, handle_root);
  gServer.on("/api/health", HTTP_GET, handle_health);
  gServer.on("/api/config", HTTP_GET, handle_config_get);
  gServer.on("/api/config", HTTP_POST, handle_config_post);
  gServer.on("/api/cloud/connect", HTTP_POST, handle_cloud_connect);
  gServer.on("/api/cloud/verify", HTTP_POST, handle_cloud_verify);
  gServer.on("/api/wifi/scan", HTTP_GET, handle_wifi_scan);
  gServer.begin();
  Serial.println("Setup portal started on http://192.168.4.1");
  while (true) {
    gServer.handleClient();
    delay(2);
  }
}

void cloud_task(void*) {
  gCloud.begin();
  while (true) gCloud.loop();
}

void display_task(void*) {
  uint32_t last_sig = 0;
  uint32_t last_forced_ms = 0;
  while (true) {
    PrinterSnapshot s = read_snapshot();
    EpaperStatusLines lines = build_epaper_status_lines(s);
    const uint32_t sig = status_signature(lines);
    if (sig != last_sig || millis() - last_forced_ms > 60000) {
      gDisplay.show_status(lines);
      last_sig = sig;
      last_forced_ms = millis();
    }
    delay(2000);
  }
}

void connect_wifi_from_store() {
  WifiCredentials wifi = gConfig.load_wifi();
  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  const String ap_ssid = String(kDeviceName) + "-Setup";
  WiFi.softAP(ap_ssid.c_str(), kSetupPassword, 1, false, 4);
  Serial.printf("Setup AP ready: SSID=%s PASS=%s IP=%s\n", ap_ssid.c_str(), kSetupPassword,
                WiFi.softAPIP().toString().c_str());
  if (!wifi.is_configured()) {
    mutate_snapshot([](PrinterSnapshot& s) {
      s.connection = PrinterConnectionState::kWaitingForCredentials;
      s.detail = "Open Web Config";
      s.setup_ap_active = true;
      s.setup_ap_ssid = std::string(kDeviceName) + "-Setup";
      s.setup_ap_ip = "192.168.4.1";
    });
    return;
  }
  WiFi.begin(wifi.ssid.c_str(), wifi.password.c_str());
  mutate_snapshot([](PrinterSnapshot& s) {
    s.connection = PrinterConnectionState::kConnecting;
    s.detail = "Connecting to Wi-Fi";
  });
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("Bootstrapping Arduino PrintSphere port");
  gStateMutex = xSemaphoreCreateMutex();
  gConfig.begin();
  connect_wifi_from_store();
  xTaskCreate(portal_task, "setup_portal", kPortalTaskStackBytes / sizeof(StackType_t), nullptr, 3,
              nullptr);
  xTaskCreate(cloud_task, "bambu_cloud", kCloudTaskStackBytes / sizeof(StackType_t), nullptr, 4,
              nullptr);
  xTaskCreate(display_task, "epaper", kDisplayTaskStackBytes / sizeof(StackType_t), nullptr, 2,
              nullptr);
}

void loop() {
  static wl_status_t last_status = WL_IDLE_STATUS;
  const wl_status_t current = WiFi.status();
  if (current != last_status) {
    last_status = current;
    mutate_snapshot([](PrinterSnapshot& s) {
      s.wifi_connected = WiFi.isConnected();
      s.wifi_ip = WiFi.isConnected() ? to_std(WiFi.localIP().toString()) : std::string{};
      if (WiFi.isConnected() && s.connection != PrinterConnectionState::kOnline) {
        s.detail = "Wi-Fi connected";
      }
    });
    if (current == WL_CONNECTED) {
      Serial.printf("Wi-Fi connected: %s\n", WiFi.localIP().toString().c_str());
      start_time_sync();
    }
  }
  delay(500);
}
