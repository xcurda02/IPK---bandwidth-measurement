/*
 * soubor: ipk-mtrip.cpp
 * autor: Vojtech Curda (xcurda02)
 *
 * IPK proj 2, varianta 1: Bandwidth measurement
 *
 * */

#include <cstring>
#include <iostream>
#include <getopt.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <zconf.h>
#include <chrono>
#include <sys/time.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <numeric>
#include <cfloat>
#include <iomanip>
#include <csignal>


///////////////////////////////// Parametry /////////////////////////////////
typedef struct Sarg{
    char *host;
    int port_num;
    ssize_t probe_size;
    int time;
}Sarg;


void paramErr(){
    std::cerr << "Invalid parameters" << std::endl;
    exit(EXIT_FAILURE);
}
int getPortNum(int argc, char **argv) {
    int c;
    int port_num = -1;
    bool first = true;
    while ((c = getopt(argc, argv, "p:")) != -1){
        switch (c){
            case '?':
                /* ignorovani prvniho parametru */
                if (!first){
                    paramErr();
                } else
                    first = false;
                break;

            case 'p': {
                char *a = nullptr;
                port_num = (int) strtol(optarg, &a, 10);
                if (*a) {
                    paramErr();
                }
            }
                break;

        }
    }
    return port_num;
}

Sarg *getArgs(int argc, char **argv){
    auto *args = new Sarg;
    args->host = nullptr;
    args->port_num = -1;
    args->probe_size = -1;
    args->time = -1;

    int c;

    bool first = true;
    while ((c = getopt(argc, argv, "h:p:s:t:")) != -1){
        switch (c){
            case '?':
                if (!first){
                    paramErr();
                } else
                    first = false;
                break;
            case 'h':
                args->host = (char *) malloc(strlen(optarg) + 1);
                strcpy(args->host, optarg);
                break;
            case 'p': {
                char *a = nullptr;
                args->port_num = (int) strtol(optarg, &a, 10);
                if (*a) {
                    paramErr();
                }
            }
                break;
            case 's': {
                char *a = nullptr;
                args->probe_size = (int) strtol(optarg, &a, 10);
                if (*a) {
                    paramErr();
                }
            }
                break;

            case 't':{
                char *a = nullptr;
                args->time = (int) strtol(optarg, &a, 10);
                if (*a) {
                    paramErr();
                }
            }
                break;
            default:
                paramErr();
        }
    }
    if (args->host != nullptr && args->time != -1 && args->probe_size != -1 && args->port_num != -1)
        return args;
    else
        paramErr();
}

/* Tisknuti hlavicky */
void printmeterArgs(Sarg *args){
    std::cout << "Reflector: " << args->host << ":" << args->port_num;
    std::cout << "    Probe size: " << args->probe_size ;
    std::cout << "    Time: " << args->time << std::endl;
    std::cout << std::setfill('-') << std::setw(80) << '-' << std::setfill(' ')<< std::endl;

}

//////////////////////////// Statistiky mereni ////////////////////////////
struct stat{
    uint64_t received_bytes;
    uint64_t packets_sent;
    uint64_t rtt_sum;           // Soucet vsech rtt v jednom behu (us)
    double Mbps;                // namerena rychlost v jednom behu (Mbps)
};

class Stats{
    std::vector<struct stat> all_stats;     // vektor pro uchovani statistik z jednotlivych behu
    struct stat current_run;                // statistiky aktuaniho behu

    /* Ziskani sumy vsech prijatych bytu ze vsech behu */
    uint64_t get_total_bytes_received(){
        uint64_t sum = 0;
        for (auto& n : all_stats)
            sum += n.received_bytes;
        return sum;
    }

    /* Ziskani prumerneho RTT ze vsech behu*/
    double get_total_avg_rtt(){
        uint64_t total_rtt_sum = 0;
        for (auto& n : all_stats)
            total_rtt_sum += n.rtt_sum;

        uint64_t total_packets_sent = 0;
        for (auto& n : all_stats)
            total_packets_sent += n.packets_sent;

        return (double) total_rtt_sum / total_packets_sent;
    }

    /* Ziskani maximalni rychlosti */
    double get_max_bw(){
        double max_bw = 0.0;
        for (auto& n : all_stats)
            max_bw = n.Mbps > max_bw ? n.Mbps : max_bw;
        return max_bw;
    }

    /* Ziskani minimalni rychlosti */
    double get_min_bw(){
        double min_bw = DBL_MAX;
        for (auto& n : all_stats)
            min_bw = n.Mbps < min_bw ? n.Mbps : min_bw;
        return min_bw;
    }

    /* Ziskani smerodatne odchylky ze vsech mereni */
    double get_std_dev(double avg){
        double std_dev = 0;
        for (auto& n : all_stats)
            std_dev += pow(n.Mbps - avg,2);

        return sqrt(std_dev/all_stats.size());
    }
public:


    bool is_empty(){
        return all_stats.empty();
    }

    /* Ukonceni jednoho behu mereni, zapsani statistik a
     * vypsani statistik aktualniho behu mereni*/
    void end_run(uint64_t bytes_received, uint64_t packets_sent, uint64_t rtt_sum){
        current_run.received_bytes = bytes_received;
        current_run.packets_sent = packets_sent;
        current_run.rtt_sum = rtt_sum;
        current_run.Mbps = (double) current_run.received_bytes * 8 / 1000000;

        std::cout << std::setw(4) << all_stats.size()+1 << ": RTT: " << std::setw(7) <<
                  (double) (current_run.rtt_sum/current_run.packets_sent) / 1000 << " ms "
                  << "\tbandwidth: " << current_run.Mbps << " Mb/s" << std::endl;

        all_stats.push_back(current_run);
    }

    /* Vypsani konecnych statistik */
    void print_end_stats(){
        double avg_Mbps = (double) ( get_total_bytes_received()*8) / (1000000 * all_stats.size());
        std::setprecision(5);
        std::cout << std::setfill('-') << std::setw(24) <<'-'<< " Bandwidth measurement statistics "<< std::setfill('-') <<
                  std::setw(22) << "-" << std::endl;

        std::cout << "Avg bandwidth : " << avg_Mbps << " Mb/s, Avg RTT: " << get_total_avg_rtt()/1000 << " ms" << std::endl;

        std::cout << "Min bandwidth: " << get_min_bw() << " Mb/s,  Max bandwidth: " << get_max_bw() << " Mb/s,  stdev: " <<
                  get_std_dev(avg_Mbps) << " Mb/s" << std::endl;
    }

};

Stats stats = Stats();      // globalni objekt uchovavajici data o jednotlivych merenich

/* V pripade preruseni z klavesnice dojde k vypsani statistik dosavadnich mereni */
void signalhandler(int signum){
    putchar('\n');
    if (!stats.is_empty())
        stats.print_end_stats();
    exit(signum);
}

int main(int argc, char **argv){
    if (argc <= 1) {
        paramErr();
    }

    /////////////////////////////////////////// METER //////////////////////////////////////////
    if (strcmp(argv[1],"meter") == 0 ){
        signal(SIGINT, signalhandler);

        Sarg *args = getArgs(argc, argv);

        if (args->probe_size < 16 || args->probe_size > 65307){
            std::cerr << "ERROR: Nepovolena velikost sondy" << std::endl;
            std::cerr << "Zadavejte pouze hodnoty v rozmezi 16..65307" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        printmeterArgs(args);               // Vytisknuti zahlavi programu

        int socketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketFD == -1){
            std::cerr << "ERROR: creating socket" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        /* Nastaveni timeoutu pro recv na 1 us*/
        struct timeval tv = {0, 1};
        if (setsockopt(socketFD, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv))) {
            perror("setsockopt");
            exit(1);
        }


        struct sockaddr_in reflector_address;
        socklen_t slen = sizeof(reflector_address);
        bzero((char *) &reflector_address, sizeof(reflector_address));
        reflector_address.sin_family = AF_INET;
        reflector_address.sin_port = htons((uint16_t)args->port_num);
        struct hostent *server = gethostbyname(args->host);
        if (server == NULL){
            std::cerr << "ERROR: invalid server address" << std::endl;
            std::exit(EXIT_FAILURE);
        }
        bcopy(server->h_addr, (char *) &reflector_address.sin_addr.s_addr, (size_t) server->h_length);


        char *buf = new char[args->probe_size];

        /* Cyklus pro rizeni jednotlivych behu */
        for (int i = 0; i < args->time+3 ; ++i) {

            time_t start = time(NULL);
            uint64_t received_bytes = 0;
            unsigned int packets_sent = 0;
            uint64_t rtt_sum = 0;

            /* Ukonceni jednoho mereni po jedne sekunde */
            while (difftime(time(NULL), start) < 1) {
                gettimeofday((struct timeval *) buf, nullptr);      // Vlozeni casoveho zaznamu do bufferu

                if (sendto(socketFD, buf, args->probe_size, 0, (struct sockaddr *) &reflector_address, slen) == -1) {
                    std::cerr << "ERROR: failed sendto" << std::endl;
                    std::exit(EXIT_FAILURE);
                }
                packets_sent++;

                char *newbuf[sizeof(struct timeval)];

                /*V pripade, ze nejsou zadna data k prijeti a po vypreseni timeoutu se pokracuje dalsi iteraci cyklu*/
                ssize_t rec = recvfrom(socketFD, &newbuf, sizeof(struct timeval), 0, (struct sockaddr *) &reflector_address, &slen);
                if ((rec == -1) && ((errno == EAGAIN) || (errno == EWOULDBLOCK))) {
                    continue;
                }

                received_bytes += args->probe_size;

                /* Porovnani casoveho zaznamu z prijateho paketu a urceni RTT */
                struct timeval sent;
                memcpy(&sent, newbuf, sizeof(struct timeval));
                struct timeval now;
                gettimeofday(&now, 0);
                int64_t rtt = ((now.tv_sec - sent.tv_sec) * 1000000) + (now.tv_usec - sent.tv_usec);
                rtt_sum += rtt;


            }

            /* Prvni tri behy se pro upresneni konecnych vysledku neberou jako validni */
            if (i > 2)
                stats.end_run(received_bytes, packets_sent, rtt_sum);

        }

        stats.print_end_stats();    // Vypsani konecnych statistik

        close(socketFD);

    ////////////////////////////////////// REFLEKTOR ///////////////////////////////////////////
    } else if ((strcmp(argv[1],"reflect") == 0) && (argc == 4)){


        int port_num = getPortNum(argc, argv);

        int socketFD = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
        if (socketFD == -1){
            std::cerr << "ERROR: failed to create socket" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        struct sockaddr_in meter_address, reflector_address;
        socklen_t slen = sizeof(meter_address);

        bzero((char *) &reflector_address, sizeof(reflector_address));

        reflector_address.sin_family = AF_INET;
        reflector_address.sin_port = htons((uint16_t) port_num);
        reflector_address.sin_addr.s_addr = htonl(INADDR_ANY);

        if( bind(socketFD , (struct sockaddr*)&reflector_address, sizeof(reflector_address) ) == -1)
        {
            std::cerr << "ERROR: failed bind" << std::endl;
            std::exit(EXIT_FAILURE);
        }

        char buf[65507];// Maximalni povolena velikost UDP paketu - (sizeof(IP Header) + sizeof(UDP Header)) = 65535-(20+8) = 65507

        std::cout << "reflecting..." << std::endl;
        while (1) {

            /* Prijeti paketu */
            if (recvfrom(socketFD, buf, sizeof(buf), 0, (struct sockaddr *) &meter_address, &slen) == -1){
                std::cerr << "ERROR: failed recv" << std::endl;
                std::exit(EXIT_FAILURE);
            }

            /* Odeslani paketu pouze s casovym zaznamem */
            char newbuf[sizeof(struct timeval)];
            memcpy(newbuf, buf, sizeof(struct timeval));
            if (sendto(socketFD, &newbuf, sizeof(struct timeval), 0, (struct sockaddr *) &meter_address, slen) == -1) {
                std::cerr << "ERROR: failed sendto" << std::endl;
                std::exit(EXIT_FAILURE);
            }


        }
        close(socketFD);


    }else{
        paramErr();
    }

    return 0;
}

