#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/bootrom.h"

#include "lwip/tcp.h"

#include "hardware/i2c.h"
#include "aht20.h"
#include "bmp280.h"
#include "ssd1306.h"
#include "font.h"

#include "FreeRTOS.h"
#include "task.h"
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "pio_matrix.pio.h"


// --- Definições ---
#define I2C_PORT i2c0
#define I2C_SDA 0
#define I2C_SCL 1
#define SEA_LEVEL_PRESSURE 101995.0
#define I2C_PORT_DISP i2c1
#define I2C_SDA_DISP 14
#define I2C_SCL_DISP 15
#define endereco 0x3C
#ifndef WIDTH
#define WIDTH 128
#endif
#ifndef HEIGHT
#define HEIGHT 64
#endif

#define WIFI_SSID "SEU SSID"
#define WIFI_PASS "SUA SENHA"


// Matriz de led's
PIO pio;
uint sm;
uint32_t VALOR_LED;
unsigned char R, G, B;
#define NUM_PIXELS 25
#define OUT_PIN 7
double v[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0, 0.1, 0.0, 0.0, 0.0};
double apaga[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
double emote[] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1, 0.1, 0.1, 0.1, 0.1, 0.1};
double x[] = {0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1, 0.0, 0.1, 0.0, 0.1, 0.0, 0.0, 0.0, 0.1};

// Leds
#define LED_BLUE_PIN 12             
#define LED_GREEN_PIN 11                
#define LED_RED_PIN 13  

// --- Variáveis Globais ---
AHT20_Data data;
struct bmp280_calib_param params;
int32_t raw_temp_bmp;
int32_t raw_pressure;
ssd1306_t ssd;

float temp_bmp_final;
float altitude_final;
float temp_aht_final;
float humidity_final;

volatile float g_altitude = 0.0f;
volatile float g_temp_bmp = 0.0f;
volatile float g_temp_aht = 0.0f;
volatile float g_temp_media = 0.0f;
volatile float g_humidity_aht = 0.0f;

volatile float g_offset_altitude = 0.0f;
volatile float g_offset_temperatura = 0.0f;
volatile float g_offset_umidade = 0.0f;


// Página HTML com CSS leve e funcional
const char HTML_BODY[] =
    "<!DOCTYPE html><html lang='pt-br'><head><meta charset='UTF-8'><title>Dashboard de Sensores</title>"
    // Carrega a biblioteca Google Charts
    "<script type='text/javascript' src='https://www.gstatic.com/charts/loader.js'></script>"
    "<style>"
    "body{font-family:Arial,sans-serif;background:#f0f0f0;margin:0;padding:20px;text-align:center;}"
    "h1,h2{color:#333;}"
    ".container{max-width:800px;margin:auto;padding:15px;background:white;border-radius:8px;box-shadow:0 2px 4px rgba(0,0,0,0.1);}"
    ".tabs button{padding:10px 15px;margin:0 5px 15px 5px;cursor:pointer;background:#ddd;border:1px solid #ccc;border-radius:4px;}"
    ".tabs button.active{background:#007bff;color:white;border-color:#007bff;}"
    ".tab-content{display:none;}"
    ".tab-content.active{display:block;}"
    ".sensor-grid{display:grid;grid-template-columns:1fr;gap:10px;margin-top:10px;}"
    ".sensor p{margin:5px 0;font-size:1.1em;}"
    ".input-group{margin:15px 0;}"
    ".input-group label{margin-right:10px;}"
    "input{padding:8px;border:1px solid #ccc;border-radius:4px;}"
    "button{cursor:pointer;}"
    // NOVO: Estilo para os contêineres dos gráficos
    ".chart-container{width:100%;height:300px;margin-top:15px;}"
    "</style>"
    "<script>"
    // --- Lógica do Gráfico ---
    "let chartAlt, chartTemp, chartHum;"
    "let dataAlt, dataTemp, dataHum;"
    "let optionsAlt, optionsTemp, optionsHum;"
    "google.charts.load('current',{'packages':['corechart']});"
    "google.charts.setOnLoadCallback(iniciarGraficos);"

    "function iniciarGraficos(){"
    // Configura dados e opções para o gráfico de Altitude
    "dataAlt=new google.visualization.DataTable();dataAlt.addColumn('string','Tempo');dataAlt.addColumn('number','Altitude');"
    "optionsAlt={title:'Altitude (m)',curveType:'function',legend:{position:'none'},vAxis:{title:'Metros'},titleTextStyle:{color:'#333'}};"
    "chartAlt=new google.visualization.LineChart(document.getElementById('chart_altitude'));"
    
    // Configura dados e opções para o gráfico de Temperatura
    "dataTemp=new google.visualization.DataTable();dataTemp.addColumn('string','Tempo');dataTemp.addColumn('number','Temperatura');"
    "optionsTemp={title:'Temperatura Média (°C)',curveType:'function',legend:{position:'none'},vAxis:{title:'°C'},titleTextStyle:{color:'#333'}};"
    "chartTemp=new google.visualization.LineChart(document.getElementById('chart_temperatura'));"
    
    // Configura dados e opções para o gráfico de Umidade
    "dataHum=new google.visualization.DataTable();dataHum.addColumn('string','Tempo');dataHum.addColumn('number','Umidade');"
    "optionsHum={title:'Umidade (%)',curveType:'function',legend:{position:'none'},vAxis:{title:'%'},titleTextStyle:{color:'#333'}};"
    "chartHum=new google.visualization.LineChart(document.getElementById('chart_umidade'));"
    "}"

    "function showTab(id,btn){document.querySelectorAll('.tab-content').forEach(t=>t.classList.remove('active'));document.querySelectorAll('.tabs button').forEach(b=>b.classList.remove('active'));document.getElementById(id).classList.add('active');btn.classList.add('active');"
    // Redesenha os gráficos quando a aba deles se torna visível
    "if(id==='graficos'){chartAlt.draw(dataAlt,optionsAlt);chartTemp.draw(dataTemp,optionsTemp);chartHum.draw(dataHum,optionsHum);}}"

    "function fetchData(){fetch('/estado').then(r=>r.json()).then(d=>{"
    // Atualiza aba Principal
    "document.getElementById('temp_media').textContent=d.temp_media_final.toFixed(2);"
    "document.getElementById('hum').textContent=d.humidity_aht_final.toFixed(2);"
    "document.getElementById('alt').textContent=d.altitude_final.toFixed(2);"
    // Atualiza aba Calibração
    "const t=document.getElementById('offset_temp');if(document.activeElement!==t)t.value=d.offset_temp.toFixed(2);"
    "const h=document.getElementById('offset_hum');if(document.activeElement!==h)h.value=d.offset_hum.toFixed(2);"
    "const a=document.getElementById('offset_alt');if(document.activeElement!==a)a.value=d.offset_alt.toFixed(2);"
    
    // NOVO: Atualiza os dados dos gráficos
    "const agora=new Date();const timestamp=agora.getHours().toString().padStart(2,'0')+':'+agora.getMinutes().toString().padStart(2,'0')+':'+agora.getSeconds().toString().padStart(2,'0');"
    "dataAlt.addRow([timestamp, d.altitude_final]);"
    "dataTemp.addRow([timestamp, d.temp_media_final]);"
    "dataHum.addRow([timestamp, d.humidity_aht_final]);"
    "const maxPontos=30;" // Mostra os últimos 30 pontos (1 minuto de dados)
    "if(dataAlt.getNumberOfRows()>maxPontos)dataAlt.removeRow(0);"
    "if(dataTemp.getNumberOfRows()>maxPontos)dataTemp.removeRow(0);"
    "if(dataHum.getNumberOfRows()>maxPontos)dataHum.removeRow(0);"
    
    // Redesenha os gráficos apenas se a aba deles estiver ativa
    "if(document.getElementById('graficos').classList.contains('active')){"
    "chartAlt.draw(dataAlt,optionsAlt);chartTemp.draw(dataTemp,optionsTemp);chartHum.draw(dataHum,optionsHum);}"
    "}).catch(e=>console.error(e));}"

    "function setOffset(tipo){const v=document.getElementById('offset_'+tipo).value;if(!isNaN(v))fetch('/offset?tipo='+tipo+'&valor='+v);}"
    "window.onload=()=>{document.getElementById('btn1').click();iniciarGraficos();setInterval(fetchData,2000);};"
    "</script>"
    "</head><body><div class='container'>"
    "<h1>Dashboard de Sensores</h1>"
    // NOVO: Botão para a aba de Gráficos
    "<div class='tabs'><button id='btn1' onclick=\"showTab('principal',this)\">Principal</button><button onclick=\"showTab('graficos',this)\">Gráficos</button><button onclick=\"showTab('calibracao',this)\">Calibração</button></div>"
    
    // Aba Principal (sem alterações)
    "<div id='principal' class='tab-content'><div class='sensor-grid'>"
    "<div class='sensor'>Temperatura Média: <b><span id='temp_media'>--</span> &deg;C</b></div>"
    "<div class='sensor'>Umidade: <b><span id='hum'>--</span> %</b></div>"
    "<div class='sensor'>Altitude: <b><span id='alt'>--</span> m</b></div>"
    "</div></div>"

    // NOVO: Conteúdo da aba de Gráficos
    "<div id='graficos' class='tab-content'>"
    "<h2>Gráficos em Tempo Real</h2>"
    "<div id='chart_temperatura' class='chart-container'></div>"
    "<div id='chart_umidade' class='chart-container'></div>"
    "<div id='chart_altitude' class='chart-container'></div>"
    "</div>"

    // Aba Calibração (sem alterações)
    "<div id='calibracao' class='tab-content'>"
    "<h2>Calibração</h2>"
    "<div class='input-group'><label>Temp:</label><input id='offset_temp' type='number' step='0.1'><button onclick=\"setOffset('temp')\">Aplicar</button></div>"
    "<div class='input-group'><label>Umid:</label><input id='offset_hum' type='number' step='0.1'><button onclick=\"setOffset('hum')\">Aplicar</button></div>"
    "<div class='input-group'><label>Alt:</label><input id='offset_alt' type='number' step='0.1'><button onclick=\"setOffset('alt')\">Aplicar</button></div>"
    "</div></div></body></html>";


// ALTERAÇÃO: Estrutura e lógica do servidor restauradas para o modelo estável
struct http_state {
    char response[10000];
    size_t len;
    size_t sent;
};

static err_t http_sent(void *arg, struct tcp_pcb *tpcb, u16_t len) {
    struct http_state *hs = (struct http_state *)arg;
    hs->sent += len;
    if (hs->sent >= hs->len) {
        tcp_close(tpcb);
        free(hs);
    }
    return ERR_OK;
}

static err_t http_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
    if (!p) { tcp_close(tpcb); return ERR_OK; }

    char *req = (char *)p->payload;
    struct http_state *hs = malloc(sizeof(struct http_state));
    if (!hs) { pbuf_free(p); tcp_close(tpcb); return ERR_MEM; }
    hs->sent = 0;

    if (strncmp(req, "GET /offset?", 12) == 0) {
        char *tipo_ptr = strstr(req, "tipo=");
        char *valor_ptr = strstr(req, "valor=");
        if (tipo_ptr && valor_ptr) {
            float valor = atof(valor_ptr + 6);
            if (strncmp(tipo_ptr + 5, "temp", 4) == 0) g_offset_temperatura = valor;
            else if (strncmp(tipo_ptr + 5, "hum", 3) == 0) g_offset_umidade = valor;
            else if (strncmp(tipo_ptr + 5, "alt", 3) == 0) g_offset_altitude = valor;
        }
        const char *txt = "OK";
        hs->len = snprintf(hs->response, sizeof(hs->response), "HTTP/1.1 200 OK\r\nConnection: close\r\n\r\n%s", txt);
    }
    else if (strstr(req, "GET /estado")) {
        char json_payload[256];
        float temp_media_final = g_temp_media + g_offset_temperatura;
        float humidity_aht_final = g_humidity_aht + g_offset_umidade;
        float altitude_final = g_altitude + g_offset_altitude;
        int json_len = snprintf(json_payload, sizeof(json_payload),
            "{\"altitude_final\":%.2f,\"temp_media_final\":%.2f,\"humidity_aht_final\":%.2f,"
            "\"offset_alt\":%.2f,\"offset_temp\":%.2f,\"offset_hum\":%.2f}",
            altitude_final, temp_media_final, humidity_aht_final,
            g_offset_altitude, g_offset_temperatura, g_offset_umidade);
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                           json_len, json_payload);
    } 
    else {
        hs->len = snprintf(hs->response, sizeof(hs->response),
                           "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: %d\r\nConnection: close\r\n\r\n%s",
                           (int)strlen(HTML_BODY), HTML_BODY);
    }

    tcp_arg(tpcb, hs);
    tcp_sent(tpcb, http_sent);
    tcp_write(tpcb, hs->response, hs->len, TCP_WRITE_FLAG_COPY);
    tcp_output(tpcb);
    pbuf_free(p);
    return ERR_OK;
}

static err_t connection_callback(void *arg, struct tcp_pcb *newpcb, err_t err) {
    tcp_recv(newpcb, http_recv);
    return ERR_OK;
}

static void start_http_server(void) {
    struct tcp_pcb *pcb = tcp_new();
    if (!pcb || tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK) {
        printf("Falha ao criar ou bindar servidor TCP\n");
        if (pcb) tcp_abort(pcb);
        return;
    }
    pcb = tcp_listen(pcb);
    tcp_accept(pcb, connection_callback);
}


// --- Lógica do Sensor e Setup ---
double calculate_altitude(double pressure) {
    return 44330.0 * (1.0 - pow(pressure / SEA_LEVEL_PRESSURE, 0.1903));
}

//rotina para definição da intensidade de cores do led
uint32_t matrix_rgb(double b, double r, double g)
{
  //unsigned char R, G, B;
  R = r * 255;
  G = g * 255;
  B = b * 255;
  return (G << 24) | (R << 16) | (B << 8);
}

// Desenha na matriz de leds em verde
void desenho_pio_green(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b){

    for (int16_t i = 0; i < NUM_PIXELS; i++) {
            valor_led = matrix_rgb(b = 0.0, r=0.0, desenho[24-i]);
            pio_sm_put_blocking(pio, sm, valor_led);
    }
}

// Desenha na matriz de leds em vermelho
void desenho_pio_red(double *desenho, uint32_t valor_led, PIO pio, uint sm, double r, double g, double b){

    for (int16_t i = 0; i < NUM_PIXELS; i++) {
            valor_led = matrix_rgb(b = 0.0, desenho[24-i], g = 0.0);
            pio_sm_put_blocking(pio, sm, valor_led);
    }
}


#define botaoB 6
void gpio_irq_handler(uint gpio, uint32_t events) {
    reset_usb_boot(0, 0);
}

void setup() {
    gpio_init(botaoB);
    gpio_set_dir(botaoB, GPIO_IN);
    gpio_pull_up(botaoB);
    gpio_set_irq_enabled_with_callback(botaoB, GPIO_IRQ_EDGE_FALL, true, &gpio_irq_handler);

    i2c_init(I2C_PORT_DISP, 400 * 1000);
    gpio_set_function(I2C_SDA_DISP, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL_DISP, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_DISP);
    gpio_pull_up(I2C_SCL_DISP);
    ssd1306_init(&ssd, WIDTH, HEIGHT, false, endereco, I2C_PORT_DISP);
    ssd1306_config(&ssd);
    ssd1306_fill(&ssd, false);
    ssd1306_send_data(&ssd);

    i2c_init(I2C_PORT, 400 * 1000);
    gpio_set_function(I2C_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C_SCL, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA);
    gpio_pull_up(I2C_SCL);

    bmp280_init(I2C_PORT);
    bmp280_get_calib_params(I2C_PORT, &params);
    aht20_reset(I2C_PORT);
    aht20_init(I2C_PORT);
}


// --- Tarefas FreeRTOS ---

void vServerTask(void *pvParameters) {
    if (cyw43_arch_init()) {
        printf("WiFi => FALHA\n");
        vTaskDelete(NULL);
    }
    cyw43_arch_enable_sta_mode();
    if (cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASS, CYW43_AUTH_WPA2_AES_PSK, 30000)) {
        printf("WiFi => ERRO\n");
        vTaskDelete(NULL);
    }
    
    printf("Conectado! Servidor HTTP rodando em: http://%s\n", ip4addr_ntoa(netif_ip4_addr(netif_default)));
    start_http_server();

    while (true) {
        cyw43_arch_poll();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void vTaskSensor(void *pvParameters) {
    char str_temp_bmp[10], str_altitude[10], str_temp_aht[10], str_humidity_aht[10];
    bool cor = true;

    while (1) {
        // Leitura dos sensores
        bmp280_read_raw(I2C_PORT, &raw_temp_bmp, &raw_pressure);
        int32_t temperature = bmp280_convert_temp(raw_temp_bmp, &params);
        int32_t pressure = bmp280_convert_pressure(raw_pressure, raw_temp_bmp, &params);
        double altitude = calculate_altitude(pressure);
        bool aht_ok = aht20_read(I2C_PORT, &data);
        
        // Atualiza as variáveis globais com os dados brutos
        g_altitude = altitude;
        g_temp_bmp = temperature / 100.0;
        if (aht_ok) {
            g_temp_aht = data.temperature;
            g_humidity_aht = data.humidity;
            g_temp_media = (g_temp_bmp + g_temp_aht) / 2.0f;
        } else {
            g_temp_aht = 0.0; g_humidity_aht = 0.0;
            g_temp_media = g_temp_bmp;
        }

        // Lógica de exibição no Display OLED com valores CORRIGIDOS
        temp_bmp_final = g_temp_bmp + g_offset_temperatura;
        altitude_final = g_altitude + g_offset_altitude;
        temp_aht_final = g_temp_aht + g_offset_temperatura;
        humidity_final = g_humidity_aht + g_offset_umidade;

        sprintf(str_temp_bmp, "%.1fC", temp_bmp_final);
        sprintf(str_altitude, "%.0fm", altitude_final);
        sprintf(str_temp_aht, "%.1fC", temp_aht_final);
        sprintf(str_humidity_aht, "%.1f%%", humidity_final);

        ssd1306_fill(&ssd, !cor);
        ssd1306_rect(&ssd, 3, 3, 122, 60, cor, !cor);
        ssd1306_line(&ssd, 3, 25, 123, 25, cor);
        ssd1306_line(&ssd, 3, 37, 123, 37, cor);
        ssd1306_draw_string(&ssd, "CEPEDI   TIC37", 8, 6);
        ssd1306_draw_string(&ssd, "EMBARCATECH", 20, 16);
        ssd1306_draw_string(&ssd, "BMP280   AHT20", 10, 28);
        ssd1306_line(&ssd, 63, 25, 63, 60, cor);
        ssd1306_draw_string(&ssd, str_temp_bmp, 14, 41);
        ssd1306_draw_string(&ssd, str_altitude, 14, 52);
        ssd1306_draw_string(&ssd, str_temp_aht, 73, 41);
        ssd1306_draw_string(&ssd, str_humidity_aht, 73, 52);
        ssd1306_send_data(&ssd);

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void vTaskMatriz(void *pvParameters) {

    // Setando matriz de leds
   double r = 0.0, b = 0.0 , g = 0.0;
   bool ok;
   ok = set_sys_clock_khz(128000, false);
   pio = pio0;

   uint offset = pio_add_program(pio, &bmp_aht_disp_program);
   uint sm = pio_claim_unused_sm(pio, true);
   bmp_aht_disp_program_init(pio, sm, offset, OUT_PIN);

    while(true) {
        if((temp_bmp_final + temp_aht_final)/2 > 23) {
            desenho_pio_red(x, VALOR_LED, pio, sm, R, G, B);
        } else {
            desenho_pio_green(v, VALOR_LED, pio, sm, R, G, B);
        }
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

void init_led(int gpio) {
    gpio_init(gpio);
    gpio_set_dir(gpio,GPIO_OUT);
}

void vTaskLed(void *pvParameters) {
    init_led(LED_BLUE_PIN);
    init_led(LED_GREEN_PIN);
    init_led(LED_RED_PIN);

        while(true) {
            if((humidity_final < 50 && humidity_final > 40) || (humidity_final > 60 && humidity_final < 70)) { // Amarelo - Alerta
                gpio_put(LED_GREEN_PIN, true);
                gpio_put(LED_RED_PIN, true);
            } else if((humidity_final < 40) || (humidity_final > 70)) { // Vermelho - Fora do ideal (muito seco ou muito úmido)
                gpio_put(LED_GREEN_PIN, false);
                gpio_put(LED_RED_PIN, true);
            } else {
                gpio_put(LED_GREEN_PIN, true);
                gpio_put(LED_RED_PIN, false);
            }
            vTaskDelay(pdMS_TO_TICKS(500));
        }
}

int main() {
    setup();
    stdio_init_all();
    sleep_ms(2000);

    // Tamanhos de stack aumentados para estabilidade
    xTaskCreate(vServerTask, "Server Task", configMINIMAL_STACK_SIZE + 2048, NULL, tskIDLE_PRIORITY + 2, NULL);
    xTaskCreate(vTaskSensor, "Sensor Task", configMINIMAL_STACK_SIZE + 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskMatriz, "Matriz Task", configMINIMAL_STACK_SIZE + 2048, NULL, tskIDLE_PRIORITY + 1, NULL);
    xTaskCreate(vTaskLed, "Led Task", configMINIMAL_STACK_SIZE + 2048, NULL, tskIDLE_PRIORITY + 1, NULL);

    vTaskStartScheduler();
    
    panic_unsupported();
    return 0;
}