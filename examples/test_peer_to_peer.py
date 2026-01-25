import sys
import time
import hashlib
import threading

try:
    # Importa a extensão nativa Hyperswarm do KadePy v0.2.1+
    from kadepy.native_hyperswarm import NativeHyperswarm
except ImportError:
    print("Erro: KadePy não encontrado ou versão antiga.")
    print("Instale/Atualize com: pip install kadepy --upgrade")
    sys.exit(1)

# Mesmo tópico do Node.js
TOPIC_STR = "kadepy-chat-v1"
TOPIC = hashlib.sha256(TOPIC_STR.encode()).digest()

class ChatPeer:
    def __init__(self):
        self.swarm = NativeHyperswarm()
        self.running = True
        
    def start(self):
        print(f"Python peer entrando no tópico: {TOPIC.hex()}")
        self.swarm.join(TOPIC)
        
        # Thread dedicada para capturar input do usuário sem bloquear o poll
        input_thread = threading.Thread(target=self.handle_input)
        input_thread.daemon = True
        input_thread.start()
        
        print("Chat iniciado! Digite e pressione Enter para enviar.")
        
        # Loop principal para processar eventos da rede (poll)
        while self.running:
            try:
                # poll() verifica eventos na rede nativa
                events = self.swarm.poll()
                if events:
                    for event in events:
                        # Imprime o que chegar. Dependendo da implementação do wrapper,
                        # o evento pode ser uma string, bytes ou objeto.
                        print(f"\n[Recebido] {event}", end='')
                        
                time.sleep(0.01) # Pequena pausa para não consumir 100% CPU
            except KeyboardInterrupt:
                self.running = False
                break
            except Exception as e:
                print(f"Erro no loop: {e}")
                time.sleep(1)
        
        print("\nDesconectando...")

    def handle_input(self):
        while self.running:
            try:
                msg = input()
                if not msg: continue
                
                # Adiciona quebra de linha para compatibilidade com o stream do Node
                data = (msg + '\n').encode('utf-8')
                
                # Envia mensagem para os peers conectados
                # Nota: send_debug é usado na implementação nativa atual para broadcast
                self.swarm.send_debug(data)
            except EOFError:
                self.running = False
                break
            except Exception as e:
                print(f"Erro ao enviar: {e}")

if __name__ == "__main__":
    peer = ChatPeer()
    peer.start()