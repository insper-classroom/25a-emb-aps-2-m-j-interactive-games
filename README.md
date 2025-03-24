# RP2040 freertos with OLED1

# Controle Customizado para Power Rangers - The Movie

## Jogo

**Power Rangers - The Movie** é um jogo de ação e aventura inspirado no universo dos Power Rangers, desenvolvido para rodar em um emulador de SNE. Nele, o jogador assume o controle de um Power Ranger que enfrenta diversos desafios e inimigos, com batalhas intensas e momentos decisivos, como a transformação do herói.

## Ideia do Controle

O controle foi especialmente desenvolvido para **Power Rangers - The Movie**. Além de atender aos requisitos básicos do projeto (botão de liga/desliga, entradas analógicas e digitais, feedback visual, etc.), a proposta inovadora é que, ao ser vibrado, o controle ative a transformação do Power Ranger no jogo. Essa funcionalidade não só adiciona um elemento extra de imersão, mas também integra feedback tátil diretamente à mecânica do jogo, promovendo uma experiência interativa e autêntica.

## Inputs e Outputs

### Entradas (Inputs)
- **Botão de Liga/Desliga:** Liga e desliga o controle.
- **2 Entradas Analógicas:** Detecta movimentos e ângulos, permitindo manobras especiais e controle preciso.
- **4 Entradas Digitais:** Botões destinados a ações do jogo, como ataque, defesa, pulo e ação especial.
- *Todas as entradas operam via callbacks e interrupções para garantir resposta imediata e sem latência.*

### Saídas (Outputs)
- **LEDs Indicadores:** Exibem o status da conexão com o emulador e sinalizam quando o controle está pronto para uso.
- **Motor de Vibração:** Fornece feedback tátil durante a jogabilidade e, ao ser ativado, dispara a transformação do Power Ranger.
- **Display (opcional):** Pode ser utilizado para exibir informações rápidas, como o estado do controle ou notificações do jogo.

## Protocolo Utilizado

- **Bluetooth:** Comunicação sem fio para transmissão eficiente dos dados entre o controle e o emulador.
- **GPIO Interrupts:** Gerencia as entradas digitais e analógicas, garantindo respostas rápidas.
- **RTOS:** Sistema operacional em tempo real que gerencia tasks, filas e semáforos, permitindo a execução paralela dos processos críticos do controle.

## Diagrama de Blocos do Firmware

### Estrutura Geral

```mermaid
flowchart TB
    subgraph Callbacks
    A[btn_note_callback]
    B[strumbar_callback]
    C[whammy_callback]
    D[imu_callback]
    end

    A --> INPUT_TASK
    B --> INPUT_TASK
    C --> INPUT_TASK
    D --> INPUT_TASK

    subgraph RTOS Tasks
    INPUT_TASK -->|xQueueAction| BT_TASK
    BT_TASK -->|xQueueFeedback| BUZZ_TASK
    BT_TASK -->|xSemaphoreBT| LED_TASK
    end

    style Callbacks fill:#EFEFEF,stroke:#999,stroke-width:1px
    style RTOS Tasks fill:#EFEFEF,stroke:#999,stroke-width:1px

    classDef task fill:#CCE5FF,stroke:#2F5597,stroke-width:1px
    classDef callback fill:#FFF2CC,stroke:#D6B656,stroke-width:1px

    class A,B,C,D callback
    class INPUT_TASK,BT_TASK,BUZZ_TASK,LED_TASK task

---

#### Principais Componentes do RTOS

- **Tasks:**
  - Leitura dos sensores (entradas analógicas e digitais)
  - Comunicação via Bluetooth com o emulador
  - Controle do motor de vibração (feedback tátil e ativação da transformação)
  - Atualização dos LEDs indicadores

- **Filas:**
  - Fila de eventos de entrada
  - Fila de comandos para o jogo
  - Fila de eventos para feedback

- **Semáforos:**
  - Controle do estado da conexão Bluetooth

- **Interrupts:**
  - Callbacks configurados para os botões e sensores, assegurando resposta imediata às interações

## Imagens do Controle

### Proposta Inicial

---

![Proposta](proposta.png)

---

## Considerações Finais

Este projeto integra os requisitos fundamentais de um controle customizado para jogos com uma funcionalidade diferenciada: a transformação do Power Ranger, acionada pelo motor de vibração. A solução desenvolvida combina tecnologias modernas e técnicas de computação embarcada, proporcionando uma experiência única e imersiva no **Power Rangers - The Movie**.
