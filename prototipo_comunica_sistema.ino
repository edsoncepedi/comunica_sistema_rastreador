#include <PubSubClient.h>  
#include <WiFi.h>
#include <Arduino.h>
#include <cstring>
#include <Wire.h>
#include <Preferences.h>

TaskHandle_t reconnect_Task; 
TaskHandle_t envio_Task; 
TaskHandle_t monitoramento_Parafusadeira_Task;

#define parafusadeira 12    // botao conectado ao GPIO 12

int numero_posto = 20;

char topico_dispositivo[40];
char topico_sistema[35];
char topico_ip[35];
char topico_mac[35];
char id_posto[15];

/* ======================================== SSID e Senha da Wifi */
const char* ssid = "gemeodigital";
const char* password = "gd@2025p";

const char* mqtt_server = "172.16.10.175";

void callback(char* topic, byte* message, unsigned int length);
void envia_dispositivo(char* msg);
void enviar_informacoes_importantes();

//Declarando objeto Wifi
WiFiClient espClient; // Cliente Wi-Fi para comunicação com o broker MQTT

//Declarando objeto MQTT
PubSubClient client(mqtt_server, 1883, callback, espClient); // Cliente MQTT usando o cliente Wi-Fi

char msg[50];  // Buffer para armazenar a mensagem

void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();
  /* ---------------------------------------- */

  //Organização de Tópicos dinamicos 
  /* ---------------------------------------- */
  snprintf(topico_dispositivo, sizeof(topico_dispositivo), "rastreio_nfc/esp32/posto_%d/dispositivo", numero_posto);
  snprintf(topico_sistema, sizeof(topico_sistema), "rastreio_nfc/esp32/posto_%d/sistema", numero_posto);
  snprintf(topico_ip, sizeof(topico_ip), "rastreio_nfc/esp32/posto_%d/ip", numero_posto);
  snprintf(topico_mac, sizeof(topico_mac), "rastreio_nfc/esp32/posto_%d/mac", numero_posto);
  snprintf(id_posto, sizeof(id_posto), "posto_%d", numero_posto);

  pinMode(parafusadeira, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  Serial.println("------------");
  Serial.print("Conectando-se a ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);

  int connecting_process_timed_out = 20; //--> 20 = 20 seconds.
  connecting_process_timed_out = connecting_process_timed_out * 2;

  while (WiFi.status() != WL_CONNECTED) {
    //Flash pisca até que a Wifi esteja conectada
    Serial.print(".");
    vTaskDelay(250);

    if(connecting_process_timed_out > 0) connecting_process_timed_out--;
    // Se o tempo de timeout for superado sem conexão o Esp será reiniciado
    if(connecting_process_timed_out == 0) {
      vTaskDelay(1000);
      ESP.restart();
    }
  }

    /* ---------------------------------------- Criando a task "reconnect_Task" usando o a função xTaskCreatePinnedToCore() */
  // Essa task é responsável pela reconecção ao Broker MQTT em caso de problemas
  xTaskCreatePinnedToCore(
             reconnect_mqtt,     /* Função da Task. */
             "reconnect_mqtt",   /* Nome da Task. */
             4096,               /* Memória destinada a Task */
             NULL,               /* Parâmetro para Task */
             2,                  /* Nível de prioridade da Task */
             &reconnect_Task,    /* Handle da Task */
             1);                 /* Núcleo onde a task é executada 0 ou 1 */
  /* ---------------------------------------- */
  
  // Essa task é responsável por monitorar o comportamento dos botões
  xTaskCreatePinnedToCore(
             monitoramento_Parafusadeira,          /* Função da Task. */
             "monitoramento_Parafusadeira",        /* Nome da Task. */
             2048,           /* Memória destinada a Task */
             NULL,           /* Parâmetro para Task */
             3,              /* Nível de prioridade da Task */
             &monitoramento_Parafusadeira_Task,    /* Handle da Task */
             0);             /* Núcleo onde a task é executada 0 ou 1 */
  /* ---------------------------------------- */ 
  
  //Espera para fazer o envio das informações importantes: IP e Mac
  vTaskDelay(1000 / portTICK_PERIOD_MS );
  Serial.println("");
  enviar_informacoes_importantes();
}

void loop() {
  vTaskDelay(100 / portTICK_PERIOD_MS );
}

// Função para reconectar ao broker MQTT caso a conexão seja perdida
void reconnect_mqtt ( void * pvParameters ) {
  // Tenta se reconectar até que a conexão seja bem-sucedida
  while(1){
    client.loop();
    while (!client.connected()) 
    {
      Serial.print("Attempting MQTT connection...");
      // Tenta conectar com o ID "camera1", informação de usuário e senha para o broker mqtt
      if (client.connect(id_posto, id_posto, "cepedi123"))
      {
        client.subscribe(topico_sistema);
        Serial.println("connected");     // Se a conexão for bem-sucedida
      } 
      else 
      {
        // Caso falhe a conexão, imprime o código de erro e tenta novamente após 5 segundos
        Serial.print("failed, rc=");
        Serial.print(client.state());
        Serial.println(" try again in 5 seconds");
        vTaskDelay( 5000 / portTICK_PERIOD_MS );  // Espera 5 segundos antes de tentar novamente
      }
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Função responsável por enviar mensagem MQTT após uma leitura
void enviar_mensagem_mqtt(void * pvParameters){
    char *msg_rec = (char *) pvParameters;

    // Se o cliente estiver conectado a mensagem será enviada, senão será enviada a menasgem de erro de conexão
    if(client.connected()) 
    {
      Serial.println("Dados enviados");
      Serial.print("Tópico: ");
      Serial.println(topico_dispositivo);
      Serial.print("Mensagem: ");
      Serial.println(msg_rec);

      client.publish(topico_dispositivo, msg_rec); // Define o tópico no qual será enviada a mensagem
    }
    else
    {
      Serial.println("Erro conexão");
    }
    vTaskDelete(NULL);
}

bool leituraAnterior_parafusadeira = true;    
bool estadoEstavel_parafusadeira = false;             // Estado final confiável
const int tempoDebounce_parafusadeira = 100;
unsigned long tempoUltimaLeitura_parafusadeira = 0;  // Quando a mudança começou

void monitoramento_Parafusadeira ( void * pvParameters ) {
  while(1){
    bool leituraAtual = digitalRead(parafusadeira);

    Serial.println("Indicando que o palete chegou no posto: deve-se enviar um BS");
    envia_dispositivo("BS");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("Indicação de leitura de palete: deve-se enviar um código de palete associado PLT01 a PLT26; É possível enviar o número de cada cartão NFC");
    envia_dispositivo("PLT01");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("Indicação de que o processo de montagem foi iniciado: deve-se enviar um BT1");
    envia_dispositivo("BT1");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("Indicação de que o processo de montagem foi encerrado: deve-se enviar um BT2");
    envia_dispositivo("BT2");
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    Serial.println("Indicando que o palete saiu do posto: deve-se enviar um BD");
    envia_dispositivo("BD");
    vTaskDelay(1000 / portTICK_PERIOD_MS);
  }
}

// Função de callback que é chamada sempre que uma nova mensagem chega a um tópico inscrito
void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");  // Imprime o tópico
  Serial.print(topic);
  Serial.print(". Message: ");
  
  String messageTemp = "";  // String temporária para armazenar a mensagem recebida

  // Imprime e armazena os dados da mensagem
  for (int i = 0; i < length; i++) { 
    messageTemp += (char)message[i]; // Adiciona o byte à string messageTemp
  }
  Serial.println("Mensagem Recebida via Mqtt");
  Serial.println(messageTemp);

}

void enviar_informacoes_importantes(){
  bool informacoes = 1;
  //Repete até que as informações sejam enviadas
  while(informacoes){
    if(client.connected()) //Só é realizada a tentativa de envio caso o client mqtt esteja pronto
      {
        Serial.println("Informações Importantes: ");
        String ip = WiFi.localIP().toString();
        String mac = WiFi.macAddress();
        Serial.print("Tópico: ");
        Serial.println(topico_ip);
        Serial.print("Mensagem: ");
        Serial.println(ip);
        Serial.print("Tópico: ");
        Serial.println(topico_mac);
        Serial.print("Mensagem: ");
        Serial.println(mac); 
        
        //Envio das informações nos tópicos dinâmicos via MQTT
        client.publish(topico_ip, ip.c_str());
        client.publish(topico_mac, mac.c_str());
        //A variável recebe false indicando que a informação foi enviada
        informacoes = 0;
      }
      else
      {
        vTaskDelay(1000 / portTICK_PERIOD_MS );
        Serial.println("Erro conexão");
    }
    }
}

void envia_dispositivo(char* msg){
    xTaskCreatePinnedToCore(
        enviar_mensagem_mqtt,     /* Função da Task. */
        "enviar_mensagem_mqtt",   /* Nome da Task. */
        20000,                    /* Memória destinada a Task */
        msg,                      /* Parâmetro para Task */
        4,                        /* Nível de prioridade da Task */
        &envio_Task,              /* Handle da Task */
        1);                       /* Núcleo onde a task é executada 0 ou 1 */
}
