# Documentação MVP para Integração de Novos Dispositivos ao Sistema

## 1. Introdução

Este documento fornece um guia para a integração de um **Dispositivo Simulador de Posto de Montagem** baseado em ESP32.  

A principal função deste firmware é enviar uma sequência cíclica e pré-definida de mensagens **MQTT**, simulando o fluxo de trabalho completo de um posto na linha de montagem.  

Ele serve como uma ferramenta de **teste e validação** para o sistema central (back-end), permitindo verificar a recepção de dados e a lógica de negócios sem a necessidade de todo o hardware físico (sensores, câmeras, etc.).

---

## 2. Requisitos de Integração

### 2.1 Requisitos de Hardware
- **Placa Controladora:** qualquer ESP32 (NodeMCU-32S, DOIT ESP32 DEVKIT V1, etc.)
- **Alimentação:** 5V DC via Micro-USB
- **Entradas Externas (Opcional):**
  - GPIO 12 definido como entrada para "parafusadeira", mas não usado na lógica de simulação

### 2.2 Requisitos de Software
- **IDE:** Arduino IDE (versão recomendada: 2.3.6)  
- **Plataforma:** suporte a ESP32 instalado na Arduino IDE  
- **Bibliotecas Arduino:**
  - [PubSubClient by Nick O’Leary (v2.8)](https://pubsubclient.knolleary.net)  
    Cliente MQTT simples para publicação/assinatura.

### 2.3 Requisitos de Rede e Servidores
- **Wi-Fi:**
  - SSID: `gemeodigital`
  - Senha: `gd@2025p`
- **MQTT Broker:**
  - IP: `172.16.10.175`
  - Porta: `1883`

---

## 3. Procedimento de Integração Passo a Passo

### Fase 1: Preparação do Ambiente e Flash do Firmware
1. Instalar **Arduino IDE** e suporte para placas ESP32.  
2. Instalar bibliotecas:  
   - Arduino IDE → `Sketch > Include Library > Manage Libraries...`  
   - Instalar **PubSubClient**.  
3. Configurar o código:  
   - Abra o arquivo do projeto.  
   - **Definir número do posto:**  
     ```cpp
     int numero_posto = 20;  // altere para o número do posto desejado
     ```
   - Atualize `ssid`, `password` e `mqtt_server` se necessário.  
4. Fazer upload do firmware:  
   - Conectar a placa ao PC.  
   - Colocar em modo **boot/flash** (segurar botão BOOT).  
   - Selecionar placa ESP32 correta na IDE.  
   - Clicar em **Upload**.

### Fase 2: Montagem do Hardware
- Apenas alimentar a placa via USB.  
- Nenhuma outra ligação necessária.

### Fase 3: Validação e Testes
1. Ligar o dispositivo (ESP32 na energia).  
2. Abrir **Monitor Serial** na Arduino IDE (`115200 baud`).  
3. Verificar logs de inicialização (Wi-Fi + MQTT conectado, IP e MAC).  
4. Conferir simulação:  
   - Dispositivo envia sequência de eventos:  

     ```
     BS -> PLT01 -> BT1 -> BT2 -> BD
     ```

   - Intervalo de 1s entre cada mensagem.  
   - Exemplo de log:  
     ```
     Dados enviados
     Tópico: rastreio/esp32/posto_20/dispositivo
     Mensagem: BS
     ```

5. Testar no cliente MQTT (ex: MQTT Box) inscrito em:  
  - rastreio/esp32/posto_20/dispositivo

---

## 4. Dicionário de Mensagens e Tópicos MQTT

O valor de `numero_posto` define os tópicos dinâmicos.  
Exemplo: se `numero_posto = 20`, os tópicos ficam `/posto_20/...`.

### Tópicos MQTT

| Tópico Base | Exemplo (`posto 20`) | Direção | Descrição |
|-------------|-----------------------|---------|-----------|
| `rastreio/esp32/posto_X/dispositivo` | `/posto_20/dispositivo` | Dispositivo → Sistema | Envia dados simulados de eventos |
| `rastreio/esp32/posto_X/sistema` | `/posto_20/sistema` | Sistema → Dispositivo | Recebe comandos (apenas exibidos no Serial) |
| `rastreio/esp32/posto_X/ip` | `/posto_20/ip` | Dispositivo → Sistema | Publica IP na inicialização |
| `rastreio/esp32/posto_X/mac` | `/posto_20/mac` | Dispositivo → Sistema | Publica MAC na inicialização |

### Sequência de Mensagens Simuladas (Payloads)

| Ordem | Mensagem | Significado |
|-------|----------|-------------|
| 1 | `BS` | Chegada do palete (sensor detecta entrada) |
| 2 | `PLT01` | Identificação do palete (QR Code ou tag) |
| 3 | `BT1` | Início do processo (acionamento da ferramenta) |
| 4 | `BT2` | Fim do processo (desacionamento da ferramenta) |
| 5 | `BD` | Saída do palete (sensor detecta saída) |

---

## Resumo
- **Fase 1:** Preparação do ambiente e upload do firmware  
- **Fase 2:** Montagem mínima do hardware (apenas USB)  
- **Fase 3:** Validação com logs e MQTT  
- **Sequência simulada:** `BS → PLT01 → BT1 → BT2 → BD`  
