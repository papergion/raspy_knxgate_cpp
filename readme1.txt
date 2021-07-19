compilazione dei sorgenti C

(le righe che iniziano con === sono dei commenti)
(le righe che iniziano con --> non vanno digitate, indicano una risposta di raspberry)

prerequisito - software GIT e librerie "paho" e "async-sockets-cpp"

Entrate in SSH e fate il login.


======================================== GIT  =============================================
== Verificare di avere già installato su raspberry il software GIT:

git –version

== Se il software GIT non è installato:

sudo apt update
sudo apt install git
git --version

======================================== PAHO =============================================
cd $home
mkdir mqttclients
cd mqttclients
sudo git clone https://github.com/janderholm/paho.mqtt.c.git
	
apt-cache policy openssl
whereis openssl
--> /usr/bin/openssl version
sudo apt-get install libssl-dev

cd paho.mqtt.c
sudo make
--> errori warnings
sudo make install

=====================================ASYNC-SOCKETS-CPP=============================================
cd $home
sudo git clone https://github.com/papergion/async-sockets-cpp.git async
sudo chown -R async
cd async

=====================================RASPY_KNXGATE_CPP=============================================
sudo mkdir knxgate
sudo git clone https://github.com/papergion/raspy_knxgate_cpp.git knxgate
cd knxgate

== ora in questa directory avete i sorgenti di raspy_knxgate - potete compilarli con il comando "make"
== gli eseguibili li troverete (dopo la compilazione) nella cartella "bin/release





per cancellare tutto:
cd $home
sudo rm -rf async