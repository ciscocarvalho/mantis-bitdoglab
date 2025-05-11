#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/adc.h"
#include "inc/server_opts.h"
#include "inc/wifi_opts.h"
#include "joystick.h"
#include "lwip/netif.h"
#include "lwip/pbuf.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"

#define BUTTON_A 5
#define BUTTON_B 6

volatile bool connected = false;
volatile bool failed = false;

bool setup_wifi() {
    // Inicializa o Wi-Fi
    while (cyw43_arch_init()) {
        printf("Falha ao iniciar Wi-Fi\n");
        sleep_ms(100);
        return false;
    }

    // Configura o Wi-Fi no modo estação
    cyw43_arch_enable_sta_mode();
    printf("Conectando ao Wi-Fi...\n");

    while (cyw43_arch_wifi_connect_timeout_ms(
        // Tenta conectar ao Wi-Fi
        WIFI_SSID,
        WIFI_PASSWORD,
        CYW43_AUTH_WPA2_AES_PSK,
        20000)) {
        printf("Falha ao conectar ao Wi-Fi\n");
        sleep_ms(100);
    }
    printf("Conectado ao Wi-Fi\n");

    return true;
}

// Quando o servidor envia uma resposta
err_t post_response(void* arg, struct tcp_pcb* pcb, struct pbuf* p, err_t err) {
    if (p != NULL) {
        printf("Resposta do servidor recebida\n");
        tcp_recved(pcb, p->tot_len);
        pbuf_free(p);
    }

    return ERR_OK;
}

err_t on_connected(void* arg, struct tcp_pcb* pcb, err_t err) {
    if (err != ERR_OK) {
        printf("Erro ao conectar: %d\n", err);
        failed = true;
        return err;
    }

    connected = true;
    return ERR_OK;
}

void post_json(char* json, char* endpoint) {
    struct tcp_pcb* pcb = tcp_new();

    if (!pcb) {
        printf("Erro ao criar PCB TCP\n");
        return;
    }

    ip_addr_t server_ip;
    ipaddr_aton(SERVER_IP, &server_ip);

    connected = false;
    failed = false;
    err_t err = tcp_connect(pcb, &server_ip, SERVER_PORT, on_connected);
    if (err != ERR_OK) {
        printf("Erro ao iniciar conexão: %d\n", err);
        tcp_abort(pcb);
        return;
    }

    for (int i = 0; i < 200; i++) {
        cyw43_arch_poll();

        if (connected || failed) {
            break;
        }

        sleep_ms(10);
    }

    if (failed || !connected) {
        printf("Conexão falhou ou expirou\n");
        tcp_abort(pcb);
        return;
    }

    char requisicao[512];
    snprintf(requisicao,
             sizeof(requisicao),
             "POST %s HTTP/1.1\r\n"
             "Host: %s:%d\r\n"
             "Content-Type: application/json\r\n"
             "Content-Length: %d\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             endpoint,
             SERVER_IP,
             SERVER_PORT,
             strlen(json),
             json);

    // Define o callback para resposta
    tcp_recv(pcb, post_response);

    err = tcp_write(pcb, requisicao, strlen(requisicao), TCP_WRITE_FLAG_COPY);
    if (err != ERR_OK) {
        printf("Erro ao enviar dados: %d\n", err);
        tcp_abort(pcb);
        return;
    }

    tcp_output(pcb);
    printf("POST enviado com o seguinte JSON: %s\n", json);
    tcp_close(pcb);
}

void post_joystick_info(JoystickInfo info) {
    char json[128];
    snprintf(json,
             sizeof(json),
             "{ \"x\": %.2f, \"y\": %.2f, \"direction\": \"%s\" }",
             info.x_normalized,
             info.y_normalized,
             info.direction);
    post_json(json, "/joystick");
}

void joystick_init() {
    adc_init();

    // Make sure GPIO is high-impedance, no pullups etc
    adc_gpio_init(26);
    adc_gpio_init(27);
}

void button_init(uint8_t button) {
    gpio_init(button);
    gpio_set_dir(button, GPIO_IN);
    gpio_pull_up(button);
}

void loading_message(char* message, int step_time, int total_time) {
    int dots = 0;
    int max_dots = 3;

    for (int i = 0, step = step_time; i < total_time; i += step) {
        printf("\r");
        for (int j = 0; j < strlen(message) + max_dots; j++) {
            printf(" ");
        }

        printf("\r%s", message);

        for (int j = 0; j < dots; j++) {
            printf(".");
        }

        fflush(stdout);

        dots = (dots + 1) % (max_dots + 1);
        sleep_ms(step);
    }

    printf("\n");
}

static float get_internal_temperature() {
    adc_select_input(4);
    const float VREF = 3.3f;
    const float ADC_CONV = VREF / 4096;
    return 27.0f - ((adc_read() * ADC_CONV) - 0.706f) / 0.001721f;
}

static void post_temperature(float temperature) {
    char json[128];
    snprintf(json, sizeof(json), "{ \"temperature\": %.2f }", temperature);
    post_json(json, "/temperature");
}

bool is_button_down(uint8_t button) {
    return gpio_get(button) == 0;
}

void post_button(uint8_t button, char* button_label, char* state) {
    char json[128];
    snprintf(json, sizeof(json), "{ \"state\": \"%s\" }", state);

    char endpoint[128];
    snprintf(endpoint, sizeof(json), "/button/%s", button_label);

    post_json(json, endpoint);
}

int main() {
    // Inicializa a entrada/saída padrão
    stdio_init_all();

    joystick_init();
    button_init(BUTTON_A);
    button_init(BUTTON_B);

    // Aguarda um tempo para iniciar o código principal e mostra uma mensagem
    // com animação. Isso é extremamente útil para evitar que o código falhe
    // antes de vermos qualquer output no serial monitor.
    loading_message("Esperando um tempo", 250, 5000);
    printf("Tempo esperado\n");

    if (!setup_wifi()) {
        return -1;
    }

    printf("Conectado à rede Wi-Fi!\n");

    if (netif_default) {
        printf("IP do dispositivo: %s\n", ipaddr_ntoa(&netif_default->ip_addr));
    }

    while (true) {
        cyw43_arch_poll();

        JoystickInfo info = joystick_get_info();
        printf("Joystick: [x: %f, y: %f, direcao: %s]\n", info.x_normalized, info.y_normalized, info.direction);
        post_joystick_info(info);

        float temperature = get_internal_temperature();
        printf("Temperature: %f\n", temperature);
        post_temperature(temperature);

        char* button_a_state = is_button_down(BUTTON_A) ? "pressed" : "unpressed";
        printf("Button A state: %s\n", button_a_state);
        post_button(BUTTON_A, "a", button_a_state);

        char* button_b_state = is_button_down(BUTTON_A) ? "pressed" : "unpressed";
        printf("Button B state: %s\n", button_b_state);
        post_button(BUTTON_A, "b", button_b_state);

        sleep_ms(500);
    }

    cyw43_arch_deinit();
    return 0;
}
