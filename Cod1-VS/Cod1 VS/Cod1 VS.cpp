/*el codigo crea 3 tablas para la base de datos:
1) equipo local: es creada, pero se deben llenar los datos de IP y nombre del equipo usado, de forma manual
2) equipos registrados: es creada, pero se deben llenar los datos de IP y nombres de equipo ubicados en la red, de forma manual
3) mensajes: es creada, se agregan mensajes recibidos y enviados

observaciones:
1) las direcciones IP para los equipos de la red deben ser fijas.
2) se usa el puerto de recepcion de mensajes es el 16
*/
#include <boost/predef.h> // Tools to identify the OS.

#ifdef BOOST_OS_WINDOWS
#define _WIN32_WINNT 0x0501
#if _WIN32_WINNT <= 0x0502 // Windows Server 2003 or earlier.
#define BOOST_ASIO_DISABLE_IOCP
#define BOOST_ASIO_ENABLE_CANCELIO
#endif
#endif

#include <sqlite3.h>

#include <boost/asio.hpp>
#include <boost/core/noncopyable.hpp>
#include <thread>
#include <mutex> //permite la sincronización entre multiples hilos (threads), protegiendo los datos que estos comparten para que no se usen al mismo tiempo
#include <memory>
#include <iostream> //libreria encargada de la entrada y salida de datos
#include <map>

#include <atomic> //usada para manejar el servidor
const unsigned int DEFAULT_THREAD_POOL_SIZE = 2; //constante para servidor

const char* dir = R"(Documentos\red_lan.db)"; //ubicacion de la base de datos

int cont_equipos_registrados{ 0 };
const int numero_equipos_en_red{ 253 };
std::string ip_equipos_registrados[numero_equipos_en_red]{};


///////////////////////////////// SECCION PARA BASE DE DATOS ///////////////////////////////////////////

static int crear_DB(const char* s)
{
	sqlite3* DB;

	int salida = 0;
	salida = sqlite3_open(s, &DB);

	sqlite3_close(DB);

	return 0;
}

static int crear_tabla(const char* s)
{
	sqlite3* DB;
	char* mensaje_error;

	std::string tabla_mensajes = "CREATE TABLE IF NOT EXISTS mensajes("
		"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
		"IP        TEXT NOT NULL, "
		"NOMBRE_PC CHAR(100), "
		"MENSAJE   CHAR(10000) );";

	std::string tabla_equipo_local = "CREATE TABLE IF NOT EXISTS equipo_local("
		"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
		"IP        TEXT NOT NULL, "
		"NOMBRE_PC CHAR(100) ); ";

	std::string tabla_equipos_registrados = "CREATE TABLE IF NOT EXISTS equipos_registrados("
		"ID INTEGER PRIMARY KEY AUTOINCREMENT, "
		"IP        TEXT NOT NULL, "
		"PUERTO    TEXT  NOT NULL, "
		"NOMBRE_PC CHAR(100) ); ";

	std::string tablas[]{ tabla_mensajes, tabla_equipo_local, tabla_equipos_registrados };
	try
	{
		for (int i = 0; i <= (sizeof(tablas) / sizeof(tablas[0])) - 1; i++)
		{
			//std::cout << "crear tabla No "<< i<< " " << tablas[i] << std::endl;
			int salida = 0;
			salida = sqlite3_open(s, &DB);
			/* An open database, SQL to be evaluated, Callback function, 1st argument to callback, Error msg written here */
			salida = sqlite3_exec(DB, tablas[i].c_str(), NULL, 0, &mensaje_error);
			if (salida != SQLITE_OK) {
				std::cerr << "Error en funcion crear_tabla" << std::endl;
				sqlite3_free(mensaje_error);
			}
			else
				std::cout << "Tabla creada exitosamente" << std::endl;
			sqlite3_close(DB);
		}

	}
	catch (const std::exception& e)
	{
		std::cerr << e.what();
	}

	return 0;
}


static int agregar_datos(const char* s, std::string ip, std::string nombre_pc, std::string mensaje)
{
	sqlite3* DB;
	char* mensaje_error;

	std::string mensajes("INSERT INTO mensajes (IP, NOMBRE_PC, MENSAJE) VALUES('" +
		ip + "', '" + nombre_pc + "', '" + mensaje + "'); ");

	int salida = sqlite3_open(s, &DB);
	/* An open database, SQL to be evaluated, Callback function, 1st argument to callback, Error msg written here */
	salida = sqlite3_exec(DB, mensajes.c_str(), NULL, 0, &mensaje_error);
	if (salida != SQLITE_OK) {
		std::cerr << "Error en agregar_datos" << std::endl;
		sqlite3_free(mensaje_error);
	}
	else
		//std::cout << "Records inserted Successfully!" << std::endl;

		return 0;
}

// retrieve contents of database used by selectData()
/* argc: holds the number of results, argv: holds each value in array, azColName: holds each column returned in array, */
static int callback_mensajes(void* NotUsed, int argc, char** argv, char** azColName)
{
	for (int i = 2; i < 4; i++) //solo iterar para mostrar las columnas NOMBRE_PC y MENSAJE
	{
		std::cout << azColName[i] << ": " << argv[i] << std::endl;
	}
	std::cout << std::endl; //COMENTAR ESTO
	return 0;
}


static int seleccionar_mensajes(const char* s)
{
	sqlite3* DB;
	char* mensaje_error;

	std::string seleccion = "SELECT * FROM mensajes;";

	int exit = sqlite3_open(s, &DB);
	/* An open database, SQL to be evaluated, Callback function, 1st argument to callback, Error msg written here*/
	exit = sqlite3_exec(DB, seleccion.c_str(), callback_mensajes, NULL, &mensaje_error);

	if (exit != SQLITE_OK) {
		std::cerr << "Error en seleccionar_mensajes" << std::endl;
		sqlite3_free(mensaje_error);
	}
	return 0;
}


// retrieve contents of database used by selectData()
/* argc: holds the number of results, argv: holds each value in array (valor nulo cuando se seleccionan columnas especificas), azColName: holds each column returned in array (contiene el valor de la columna cuando se seleccionan columnas especificas), */
static int callback_equipo_local(void* p_columna_equipo_local, int argc, char** argv, char** azColName)
{
	std::string nombre_ip_local{ azColName[1] };

	*((std::string*)p_columna_equipo_local) = azColName[1];

	std::cout << std::endl;
	return 0;
}


static int seleccionar_equipo_local(const char* s, std::string* p_ip_equipo_local, std::string* p_nombre_pc_equipo_local)
{
	sqlite3* DB;
	char* mensaje_error{};

	std::string seleccion_ip = "SELECT IP FROM equipo_local;";
	std::string seleccion_nombre_pc = "SELECT NOMBRE_PC FROM equipo_local;";
	std::string selecciones[]{ seleccion_ip, seleccion_nombre_pc };

	std::string ip_equipo_local, nombre_pc_equipo_local;

	for (int i = 0; i <= (sizeof(selecciones) / sizeof(selecciones[0])) - 1; i++)
	{
		int exit = sqlite3_open(s, &DB);

		switch (i)
		{
		case 0:
			/* An open database, SQL to be evaluated, Callback function, 1st argument to callback, Error msg written here*/
			exit = sqlite3_exec(DB, selecciones[i].c_str(), callback_equipo_local, &ip_equipo_local, &mensaje_error);
			*p_ip_equipo_local = ip_equipo_local;
			//std::cout << "ip_equipo_local: " << ip_equipo_local << std::endl;
			break;
		case 1:
			exit = sqlite3_exec(DB, selecciones[i].c_str(), callback_equipo_local, &nombre_pc_equipo_local, &mensaje_error);
			*p_nombre_pc_equipo_local = nombre_pc_equipo_local;
			//std::cout << "nombre_pc_equipo_local: " << nombre_pc_equipo_local << std::endl;
			break;
		}

		if (exit != SQLITE_OK) {
			std::cerr << "Error en seleccionar_equipo_local" << std::endl;
			sqlite3_free(mensaje_error);
		}
	}
	return 0;
}


static int callback_equipos_registrados(void* p_columna_equipo_local, int argc, char** argv, char** azColName)
{
	std::cout << azColName[1] << ": " << argv[0] << std::endl;
	ip_equipos_registrados[cont_equipos_registrados] = ((std::string)argv[0]);

	std::string nombre_ip_local{ azColName[1] };

	*((std::string*)p_columna_equipo_local) = azColName[1];

	std::cout << "equipos registrados: "<< ip_equipos_registrados[cont_equipos_registrados]<<std::endl;
	cont_equipos_registrados++;
	return 0;
}


static int seleccionar_equipos_registrados(const char* s)
{
	sqlite3* DB;
	char* mensaje_error{};

	std::string seleccion_ip = "SELECT PUERTO FROM equipos_registrados;";
	std::string ip_equipo_local{};

	int exit = sqlite3_open(s, &DB);

		/* An open database, SQL to be evaluated, Callback function, 1st argument to callback, Error msg written here*/
		exit = sqlite3_exec(DB, seleccion_ip.c_str(), callback_equipos_registrados, &ip_equipo_local, &mensaje_error);


		if (exit != SQLITE_OK) {
			std::cerr << "Error en seleccionar_equipo_local" << std::endl;
			sqlite3_free(mensaje_error);
		}
return 0;
}


///////////////////////////////// SECCION PARA CLIENTE ///////////////////////////////////////////

// Function pointer type that points to the callback
// function which is called when a request is complete.
typedef void(*Callback) (unsigned int request_id,
	const std::string& response,
	const boost::system::error_code& ec);

//std::string mensaje_cliente; //mensaje ingresado por el cliente desde la consola


// Structure represents a context of a single request.
struct Session {
	Session(boost::asio::io_service& ios,
		const std::string& raw_ip_address,
		unsigned short port_num,
		const std::string& request,
		unsigned int id,
		Callback callback) :
		m_sock(ios),
		m_ep(boost::asio::ip::address::from_string(raw_ip_address),
			port_num),
		m_request(request),
		m_id(id),
		m_callback(callback),
		m_was_cancelled(false) {}
	boost::asio::ip::tcp::socket m_sock; // Socket used for communication
	boost::asio::ip::tcp::endpoint m_ep; // Remote endpoint.
	std::string m_request; // Request string.
	// streambuf where the response will be stored.
	boost::asio::streambuf m_response_buf;
	std::string m_response; // Response represented as a string.
	// Contains the description of an error if one occurs during
	// the request life cycle.
	boost::system::error_code m_ec;
	unsigned int m_id; // Unique ID assigned to the request.
	// Pointer to the function to be called when the request
	// completes.
	Callback m_callback;
	bool m_was_cancelled;
	std::mutex m_cancel_guard;
};


class AsyncTCPClient : public boost::noncopyable
{
public:
	AsyncTCPClient() {
		m_work.reset(new boost::asio::io_service::work(m_ios));
		m_thread.reset(new std::thread([this]() {
			m_ios.run();
			}));
	}

	void envio_de_mensaje(
		const std::string& raw_ip_address,
		unsigned short port_num,
		Callback callback,
		unsigned int request_id, std::string mensaje_cliente) {
		// Preparing the request string.
		std::string request = mensaje_cliente;
		std::shared_ptr<Session> session =
			std::shared_ptr<Session>(new Session(m_ios,
				raw_ip_address,
				port_num,
				request,
				request_id,
				callback));

		std::unique_lock<std::mutex>
			lock(m_active_sessions_guard);

		m_active_sessions[request_id] = session;
		lock.unlock();
		session->m_sock.async_connect(session->m_ep,
			[this, session](const boost::system::error_code& ec)
			{
				if (ec.value() != 0) {
					session->m_ec = ec;
					onRequestComplete(session);
					return;
				}
				std::unique_lock<std::mutex>
					cancel_lock(session->m_cancel_guard);
				if (session->m_was_cancelled) {
					onRequestComplete(session);
					return;
				}
				boost::asio::async_write(session->m_sock,
					boost::asio::buffer(session->m_request),
					[this, session](const boost::system::error_code& ec,
						std::size_t bytes_transferred)
					{
						if (ec.value() != 0) {
							session->m_ec = ec;
							onRequestComplete(session);
							return;
						}
						std::unique_lock<std::mutex>
							cancel_lock(session->m_cancel_guard);
						if (session->m_was_cancelled) {
							onRequestComplete(session);
							return;
						}
						boost::asio::async_read_until(session->m_sock,
							session->m_response_buf,
							'\n',
							[this, session](const boost::system::error_code& ec,
								std::size_t bytes_transferred)
							{
								if (ec.value() != 0) {
									session->m_ec = ec;
								}
								else {
									std::istream strm(&session->m_response_buf);
									std::getline(strm, session->m_response);
								}
								onRequestComplete(session);
							}); }); });
	};

	void close() {
		// Destroy work object. This allows the I/O thread to
		// exits the event loop when there are no more pending
		// asynchronous operations.
		m_work.reset(NULL);
		// Wait for the I/O thread to exit.
		m_thread->join();
	}


private:
	void onRequestComplete(std::shared_ptr<Session> session) {
		// Shutting down the connection. This method may
		// fail in case socket is not connected. We don’t care
		// about the error code if this function fails.
		boost::system::error_code ignored_ec;
		session->m_sock.shutdown(
			boost::asio::ip::tcp::socket::shutdown_both,
			ignored_ec);
		// Remove session form the map of active sessions.
		std::unique_lock<std::mutex>
			lock(m_active_sessions_guard);
		auto it = m_active_sessions.find(session->m_id);
		if (it != m_active_sessions.end())
			m_active_sessions.erase(it);
		lock.unlock();
		boost::system::error_code ec;
		if (session->m_ec.value() == 0 && session->m_was_cancelled)
			ec = boost::asio::error::operation_aborted;
		else
			ec = session->m_ec;
		// Call the callback provided by the user.
		session->m_callback(session->m_id,
			session->m_response, ec);
	};
private:
	boost::asio::io_service m_ios;
	std::map<int, std::shared_ptr<Session>> m_active_sessions;
	std::mutex m_active_sessions_guard;
	std::unique_ptr<boost::asio::io_service::work> m_work;
	std::unique_ptr<std::thread> m_thread;
};



void handler(unsigned int request_id,
	const std::string& response,
	const boost::system::error_code& ec)
{
	if (ec.value() == 0) {

	}
	else {
		std::cout << " Envio fallido. codigo de error= " << ec.value()
			<< ". Mensaje de error = " << ec.message()
			<< std::endl;
	}
	return;
}



///////////////////////////////// SECCION PARA SERVIDOR ///////////////////////////////////////////
class Service {
public:
	Service(std::shared_ptr<boost::asio::ip::tcp::socket> sock) :
		m_sock(sock)
	{}
	void StartHandling() {
		boost::asio::async_read_until(*m_sock.get(),
			m_request,
			'\n',
			[this](
				const boost::system::error_code& ec,
				std::size_t bytes_transferred)
			{
				onRequestReceived(ec,
					bytes_transferred);
			});
	}
private:
	void onRequestReceived(const boost::system::error_code& ec,
		std::size_t bytes_transferred) {
		if (ec.value() != 0) {
			std::cout << "Error occured! Error code = "
				<< ec.value()
				<< ". Message: " << ec.message();
			onFinish();
			return;
		}
		// Process the request.
		m_response = ProcessRequest(m_request);
		// Initiate asynchronous write operation.
		boost::asio::async_write(*m_sock.get(),
			boost::asio::buffer(m_response),
			[this](
				const boost::system::error_code& ec,
				std::size_t bytes_transferred)
			{
				onResponseSent(ec,
					bytes_transferred);
			});
	}
	void onResponseSent(const boost::system::error_code& ec,
		std::size_t bytes_transferred) {
		if (ec.value() != 0) {
			std::cout << "Error occured! Error code = "
				<< ec.value()
				<< ". Message: " << ec.message();
		}
		onFinish();
	}
	// Here we perform the cleanup.
	void onFinish() {
		delete this;
	}
	std::string ProcessRequest(boost::asio::streambuf& request) {
		//convertir a string la solicitud
		std::string s((std::istreambuf_iterator<char>(&request)), std::istreambuf_iterator<char>());

		std::size_t pos = s.find('\0'); //encontrar la ubicacion del caracter nulo
		std::string mensaje_oculto = s.substr(pos + 1); //crear substring a partir de dicha posicion de caracter nulo, excluyendolo

		char contenido[100]{}, ip_array[30]{}, nombre_pc_array[70]{};
		char* contexto{  };

		strcpy_s(contenido, mensaje_oculto.c_str());

		strcpy_s(ip_array, sizeof ip_array, strtok_s(contenido, "-", &contexto));
		strcpy_s(nombre_pc_array, sizeof nombre_pc_array, strtok_s(NULL, "\n", &contexto));

		std::string ip_string{ ip_array }, nombre_pc_string{ nombre_pc_array };
		//std::cout << "ip_string: " << ip_string << std::endl;
		std::cout << "NOMBRE_PC: " << nombre_pc_string << std::endl;

		std::string mensaje_impresion = s.substr(0, pos);
		std::cout << "MENSAJE: " << mensaje_impresion << std::endl;

		agregar_datos(dir, ip_string, nombre_pc_string, mensaje_impresion);

		std::string response = "OK\n";

		return response;
	}
private:
	std::shared_ptr<boost::asio::ip::tcp::socket> m_sock;
	std::string m_response;
	boost::asio::streambuf m_request;
};


class Acceptor {
public:
	Acceptor(boost::asio::io_service& ios, unsigned short port_num) :
		m_ios(ios),
		m_acceptor(m_ios,
			boost::asio::ip::tcp::endpoint(
				boost::asio::ip::address_v4::any(),
				port_num)),
		m_isStopped(false)
	{}
	// Start accepting incoming connection requests.
	void Start() {
		m_acceptor.listen();
		InitAccept();
	}
	// Stop accepting incoming connection requests.
	void Stop() {
		m_isStopped.store(true);
	}
private:
	void InitAccept() {
		std::shared_ptr<boost::asio::ip::tcp::socket>
			sock(new boost::asio::ip::tcp::socket(m_ios));
		m_acceptor.async_accept(*sock.get(),
			[this, sock](const boost::system::error_code& error)
			{
				onAccept(error, sock);
			});
	}
	void onAccept(const boost::system::error_code& ec,
		std::shared_ptr<boost::asio::ip::tcp::socket> sock)
	{
		if (ec.value() == 0) {
			(new Service(sock))->StartHandling();
		}
		else {
			std::cout << "Error occured! Error code = "
				<< ec.value()
				<< ". Message: " << ec.message();
		}
		// Init next async accept operation if
		// acceptor has not been stopped yet.
		if (!m_isStopped.load()) {
			InitAccept();
		}
		else {
			// Stop accepting incoming connections
			// and free allocated resources.
			m_acceptor.close();
		}
	}
private:
	boost::asio::io_service& m_ios;
	boost::asio::ip::tcp::acceptor m_acceptor;
	std::atomic<bool>m_isStopped;
};


class Server {
public:
	Server() {
		m_work.reset(new boost::asio::io_service::work(m_ios));
	}
	// Start the server.
	void Start(unsigned short port_num,
		unsigned int thread_pool_size) {
		assert(thread_pool_size > 0);
		// Create and start Acceptor.
		acc.reset(new Acceptor(m_ios, port_num));
		acc->Start();
		// Create specified number of threads and
		// add them to the pool.
		for (unsigned int i = 0; i < thread_pool_size; i++) {
			std::unique_ptr<std::thread> th(
				new std::thread([this]()
					{
						m_ios.run();
					}));
			m_thread_pool.push_back(std::move(th));
		}
	}
	// Stop the server.
	void Stop() {
		acc->Stop();
		m_ios.stop();
		for (auto& th : m_thread_pool) {
			th->join();
		}
	}
private:
	boost::asio::io_service m_ios;
	std::unique_ptr<boost::asio::io_service::work>m_work;
	std::unique_ptr<Acceptor>acc;
	std::vector<std::unique_ptr<std::thread>>m_thread_pool;
};



int main()
{

	std::string mensaje_cliente; //mensaje ingresado por el cliente desde la consola
	unsigned short numero_puerto = 16; //puerto de salida de datos

	std::string ip_equipo_local{}, nombre_pc_equipo_local{}; //variables para almacenar valores de la base de datos

	crear_DB(dir);
	crear_tabla(dir);
	seleccionar_mensajes(dir);
	seleccionar_equipo_local(dir, &ip_equipo_local, &nombre_pc_equipo_local);
	seleccionar_equipos_registrados(dir);

	Server srv;
	unsigned int thread_pool_size =
		std::thread::hardware_concurrency() * 2;
	if (thread_pool_size == 0)
		thread_pool_size = DEFAULT_THREAD_POOL_SIZE;

	srv.Start(numero_puerto, thread_pool_size);

	while (1)
	{
		try {

			AsyncTCPClient client;

			getline(std::cin, mensaje_cliente);

			agregar_datos(dir, ip_equipo_local, nombre_pc_equipo_local, mensaje_cliente);
			std::string mensaje_a_enviar(mensaje_cliente + "\0" + ip_equipo_local + "-" + nombre_pc_equipo_local);

			std::cout << "mensaje_cliente " << mensaje_a_enviar << std::endl;
			for (int i = 0; i <= cont_equipos_registrados-1; i++)
			{
				client.envio_de_mensaje(ip_equipos_registrados[i], numero_puerto,
					handler, 1, mensaje_a_enviar);
			}
			cont_equipos_registrados = 0;
			for (int i = 0; i < numero_equipos_en_red; i++) {
				ip_equipos_registrados[i].clear();
			}
			client.close();
		}
		catch (boost::system::system_error& e) {
			std::cout << "Error occured! Error code = " << e.code()
				<< ". Message: " << e.what();
			return e.code().value();
		}
	}
	return 0;
};