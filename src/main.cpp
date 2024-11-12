#include <iostream>
#include <thread>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <semaphore>
#include <atomic>
#include <chrono>
#include <random>
#include <array>
#include <algorithm>

// Global variables for synchronization
constexpr int NUM_JOGADORES = 4;
std::counting_semaphore<NUM_JOGADORES> cadeira_sem(NUM_JOGADORES - 1); // Inicia com n-1 cadeiras, capacidade máxima n
std::condition_variable music_cv;
std::mutex music_mutex;
std::mutex estado_mutex;
std::atomic<bool> jogo_ativo{true};
bool fim_rodada = false;
bool musica_parada = false;
int linha=0;
std::random_device rd;
std::mt19937 gen(rd());
std::uniform_int_distribution<> distrib(1000, 4000);

std::array<int, NUM_JOGADORES> jogadores_ativos;

void print_line(int linha){
    std::cout<<"Teste aqui linha "<< linha <<"\n";
}

/*
 * Uso básico de um counting_semaphore em C++:
 * 
 * O std::counting_semaphore é um mecanismo de sincronização que permite controlar o acesso a um recurso compartilhado 
 * com um número máximo de acessos simultâneos. Neste projeto, ele é usado para gerenciar o número de cadeiras disponíveis.
 * Inicializamos o semáforo com n - 1 para representar as cadeiras disponíveis no início do jogo. 
 * Cada jogador que tenta se sentar precisa fazer um acquire(), e o semáforo permite que até n - 1 jogadores 
 * ocupem as cadeiras. Quando todos os assentos estão ocupados, jogadores adicionais ficam bloqueados até que 
 * o coordenador libere o semáforo com release(), sinalizando a eliminação dos jogadores.
 * O método release() também pode ser usado para liberar múltiplas permissões de uma só vez, por exemplo: cadeira_sem.release(3);,
 * o que permite destravar várias threads de uma só vez, como é feito na função liberar_threads_eliminadas().
 *
 * Métodos da classe std::counting_semaphore:
 * 
 * 1. acquire(): Decrementa o contador do semáforo. Bloqueia a thread se o valor for zero.
 *    - Exemplo de uso: cadeira_sem.acquire(); // Jogador tenta ocupar uma cadeira.
 * 
 * 2. release(int n = 1): Incrementa o contador do semáforo em n. Pode liberar múltiplas permissões.
 *    - Exemplo de uso: cadeira_sem.release(2); // Libera 2 permissões simultaneamente.
 */

// Classes
class JogoDasCadeiras {
private:
    int num_jogadores;
    int cadeiras; 
    int cadeiras_ocupadas = 0;  
    int jogador_eliminado = 0;
    int vencedor=0;
    std::array<int, NUM_JOGADORES> estado;    
    std::vector<int> jogadores_ativos;
    bool primeira_rodada = true;
public:
    JogoDasCadeiras(int num_jogadores) {
        this->num_jogadores = num_jogadores;
        this->cadeiras = num_jogadores-1;
        this->cadeiras_ocupadas = 0;
        estado.fill(0); // Inicializa corretamente o array
        for(int i = 0; i < this->num_jogadores; i++) {
            jogadores_ativos.push_back(i+1);
        }
    }

    void iniciar_rodada() {
        std::lock_guard<std::mutex> lock(estado_mutex);
        if(!primeira_rodada){
            this->num_jogadores--;     
            this->cadeiras = this->num_jogadores-1; 
            this->cadeiras_ocupadas = 0;   
            this->jogador_eliminado = 0;
            std::cout<<"\nProxima rodada com "<<num_jogadores<<" jogadores e "<<cadeiras<<" cadeiras.\n A música está tocando... 🎵"<<std::endl;
        }else{
            this->primeira_rodada=false;   
            std::cout<<"\nIniciando rodada com "<<num_jogadores<<" jogadores e "<<cadeiras<<" cadeiras.\n A música está tocando... 🎵"<<std::endl; 
        }
        for(int i=0; i<this->estado.size();i++){
            this->estado[i]=0;
        }
    }

    void parar_musica() {
        std::lock_guard<std::mutex> lock(music_mutex);
        musica_parada = true;
        music_cv.notify_all();
    }

    void eliminar_jogador(int jogador_id) {
        for(int i = 0; i<this->jogadores_ativos.size(); i++) {
            if(this->jogadores_ativos[i]==jogador_id){
                this->jogadores_ativos[i]=0;
                this->jogador_eliminado = jogador_id;
                fim_rodada = true;
                music_cv.notify_all();
                break;
            }
        }
    }

    void ocupar_cadeira(int jogador_id) {
        if(this->cadeiras_ocupadas<this->cadeiras){
            
            if(this->estado[jogador_id] == 0) {
                this->cadeiras_ocupadas++;
                this->estado[cadeiras_ocupadas] = jogador_id;
                this->vencedor=jogador_id;
            }

        }else{
            this->eliminar_jogador(jogador_id);
        }
        return;
    }

    void exibir_estado() {
        for(int i = 1; i < estado.size(); i++) {
            if(estado[i] != 0) {
                std::cout << "[Cadeira " << i << "]: Ocupada por P" << estado[i] << "\n";
            }
        }
        std::cout<<std::endl;
        std::cout <<"Jogador P" << this->jogador_eliminado << " não conseguiu uma cadeira e foi eliminado!\n"<<"-----------------------------------------------\n";
    }

    bool esta_ativo(int jogador_id) {
        std::lock_guard<std::mutex> lock(estado_mutex);
        for(int i = 0; i<this->jogadores_ativos.size(); i++) {
            if(this->jogadores_ativos[i]==jogador_id){
                return true;
            }
        }
        return false;
    }
    int get_qtd_cadeira() { return this->cadeiras; }
    int get_vencedor(){ return this->vencedor;}
};

class Jogador {
public:
    Jogador(int id, JogoDasCadeiras& jogo)
        : id(id), jogo(jogo), ativo(true) {}

    void joga() {
        while(ativo && jogo_ativo) {
            {
                std::unique_lock<std::mutex> lock(music_mutex);
                music_cv.wait(lock, [] { return musica_parada && !fim_rodada; });
                if (cadeira_sem.try_acquire()) {
                    this->jogo.ocupar_cadeira(id);
                } else {
                    this->jogo.eliminar_jogador(id);
                }
                if (!this->jogo.esta_ativo(id)) {
                    this->ativo = false;
                    break;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return;
    }


private:
    int id;
    JogoDasCadeiras& jogo;
    bool ativo;
};

class Coordenador {
public:
    Coordenador(JogoDasCadeiras& jogo)
        : jogo(jogo) {}

    void iniciar_jogo() {
        // TODO: Começa o jogo, dorme por um período aleatório, e então para a música, sinalizando os jogadores 
        std::cout << "-----------------------------------------------\nBem-vindo ao Jogo das Cadeiras Concorrente!\n-----------------------------------------------"<<std::endl;
        while(jogo_ativo){
            {
                this->jogo.iniciar_rodada();
                std::this_thread::sleep_for(std::chrono::milliseconds(distrib(gen)));
                std::cout<<"\n> A música parou! Os jogadores estão tentando se sentar..."<<std::endl;
                std::cout << "\n-----------------------------------------------\n";
                this->jogo.parar_musica();
                std::unique_lock<std::mutex> lock(music_mutex);
                music_cv.wait(lock, [] { return fim_rodada; });
                musica_parada = false;
                this->jogo.exibir_estado();
                this->liberar_threads_eliminadas();
                fim_rodada = false;
            }
            if(this->jogo.get_qtd_cadeira() == 1){
                jogo_ativo = false;
                break;
            }
        }

    }
    void liberar_threads_eliminadas() {
        // Libera múltiplas permissões no semáforo para destravar todas as threads que não conseguiram se sentar
        std::lock_guard<std::mutex> lock(estado_mutex);
        cadeira_sem.release(this->jogo.get_qtd_cadeira() - 1); // Libera o número de permissões igual ao número de jogadores que ficaram esperando
    }

private:
    JogoDasCadeiras& jogo;
};

// Main function
int main() {
    JogoDasCadeiras jogo(NUM_JOGADORES);

    Coordenador coordenador(jogo);
    std::vector<std::thread> jogadores;

    // Criação das threads dos jogadores
    std::vector<Jogador> jogadores_objs;
    for (int i = 1; i <= NUM_JOGADORES; ++i) {
        jogadores_objs.emplace_back(i, jogo);
    }

    for (int i = 0; i < NUM_JOGADORES; ++i) {
        jogadores.emplace_back(&Jogador::joga, &jogadores_objs[i]);
    }

    // Thread do coordenador
    std::thread coordenador_thread(&Coordenador::iniciar_jogo, &coordenador);

    // Esperar pelas threads dos jogadores
    for (auto& t : jogadores) {
        if (t.joinable()) {
            t.join();
        }
    }
    // Esperar pela thread do coordenador
    if (coordenador_thread.joinable()) {
        coordenador_thread.join();
    }
    std::cout <<"\n🏆 Vencedor: Jogador P"<<jogo.get_vencedor()<<"! Parabéns! 🏆\n"<<"-----------------------------------------------\n\nObrigado por jogar o Jogo das Cadeiras Concorrente!"<<std::endl;
    return 0;
}

//g++ -std=c++20 main.cpp -pthread -o jogo_cadeiras