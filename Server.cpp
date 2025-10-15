#include <iostream>
#include <fstream>
#include <string>
#include <cstring>

// Bibliotecas Winsock para Windows
#include <winsock2.h>
#include <ws2tcpip.h>

// Biblioteca de parsing JSON
#include "json.hpp" 
using json = nlohmann::json;

#pragma comment(lib, "ws2_32.lib") 

const int PORT = 8888; 
const int BUFFER_SIZE = 4096;
const std::string STORAGE_FILENAME = "calibrations.json"; // Arquivo mestre de armazenamento

// ------------------------------------------------------------------
// Função para carregar o JSON existente do disco
// ------------------------------------------------------------------
json loadExistingData() {
    std::ifstream infile(STORAGE_FILENAME);
    if (infile.is_open()) {
        try {
            // Tenta ler e parsear o JSON do arquivo
            json data = json::parse(infile);
            infile.close();
            return data;
        } catch (const json::parse_error& e) {
            std::cerr << "AVISO: Arquivo " << STORAGE_FILENAME << " invalido. Criando novo." << std::endl;
            infile.close();
        }
    }
    // Retorna um objeto JSON vazio se o arquivo não existir ou for inválido
    return json::object();
}

// ------------------------------------------------------------------
// Função para salvar o objeto JSON completo no disco
// ------------------------------------------------------------------
void saveConsolidatedData(const json& consolidatedData) {
    std::ofstream outfile(STORAGE_FILENAME);

    if (!outfile.is_open()) {
        std::cerr << "ERRO: Nao foi possivel abrir o arquivo " << STORAGE_FILENAME << " para escrita." << std::endl;
        return;
    }

    // Escreve o objeto JSON no arquivo com indentação de 4 espaços para fácil leitura
    outfile << consolidatedData.dump(4);
    outfile.close();

    std::cout << "SUCESSO: Dados consolidados salvos em " << STORAGE_FILENAME << std::endl;
}

// ------------------------------------------------------------------
// Manipulador principal de pacotes UDP
// ------------------------------------------------------------------
void handleUdpPacket(const std::string& json_string) {
    try {
        // 1. Tenta fazer o parsing da string JSON
        json incomingData = json::parse(json_string);
        
        std::cout << "JSON PARSEADO com sucesso. Atualizando dados existentes..." << std::endl;

        // 2. Carrega o arquivo JSON de calibração existente
        json consolidatedData = loadExistingData();

        // 3. Itera sobre os dados recebidos (deve ter apenas uma chave raiz: 'atacante', 'defender', etc.)
        for (auto const& [key, value] : incomingData.items()) {
            std::string vertente = key; 
            
            if (value.is_object()) {
                std::cout << "Vertente atualizada: '" << vertente << "'" << std::endl;
                
                // 4. Substitui/Atualiza a vertente específica no objeto consolidado
                consolidatedData[vertente] = value;
                
                // 5. Salva o objeto JSON consolidado de volta no disco
                saveConsolidatedData(consolidatedData);
                return; 
            }
        }
        
        std::cerr << "AVISO: JSON recebido nao contem uma vertente valida (ex: 'atacante')." << std::endl;

    } catch (const json::parse_error& e) {
        std::cerr << "ERRO de Parsing JSON: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "ERRO geral no processamento: " << e.what() << std::endl;
    }
}


int main() {
    WSADATA wsaData;
    SOCKET sockfd;
    char buffer[BUFFER_SIZE];
    struct sockaddr_in servaddr, cliaddr;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup falhou." << std::endl;
        return 1;
    }

    if ((sockfd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET) {
        std::cerr << "Falha na criacao do socket. Erro: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    servaddr.sin_port = htons(PORT);

    if (bind(sockfd, (const struct sockaddr *)&servaddr, sizeof(servaddr)) == SOCKET_ERROR) {
        std::cerr << "Falha ao vincular (bind). Erro: " << WSAGetLastError() << std::endl;
        closesocket(sockfd);
        WSACleanup();
        return 1;
    }

    std::cout << "Servidor UDP escutando na porta " << PORT << "..." << std::endl;
    std::cout << "Arquivo de armazenamento: " << STORAGE_FILENAME << std::endl;
    std::cout << "-------------------------------------------------" << std::endl;
    
    while (true) {
        int len = sizeof(cliaddr);
        
        int n = recvfrom(
            sockfd, 
            buffer, 
            BUFFER_SIZE, 
            0, 
            (struct sockaddr *)&cliaddr, 
            &len
        );

        if (n == SOCKET_ERROR) {
            // Ignora erros comuns de interrupção
            continue;
        }

        buffer[n] = '\0';
        std::string json_string(buffer); 

        // Chama o manipulador para processar o JSON e salvar
        handleUdpPacket(json_string);
    }

    closesocket(sockfd);
    WSACleanup(); 
    return 0;
}