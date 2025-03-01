// definition of endianess (e.g. needed on raspberry pi)
#define PJ_IS_LITTLE_ENDIAN 1
#define PJ_IS_BIG_ENDIAN 0

// disable pjsua logging
#define PJSUA_LOG_LEVEL 0

// includes
#include <signal.h>
#include <pjsua-lib/pjsua.h>

// struct for app configuration settings
struct app_config { 
	char *sip_domain;
	char *sip_user;
	char *sip_password;
	int sip_port;
	char *phone_number;
	char *audio_file;
	int repetition_limit;
	int silent_mode;
	int timeout;
} app_cfg;

// global helper vars
int call_confirmed = 0;
int media_counter = 0;
int error_code = -1;

// global vars for pjsua
pjsua_acc_id acc_id;
pjsua_player_id play_id = PJSUA_INVALID_ID;
pjmedia_port *play_port;

// header of helper-methods
static void create_player(pjsua_call_id);
static void log_message(char *);
static void make_sip_call();
static void setup_sip(void);
static void register_sip(void);
static void usage(int);
static int try_get_argument(int, char *, char **, int, char *[]);

// header of callback-methods
static void on_call_media_state(pjsua_call_id);
static void on_call_state(pjsua_call_id, pjsip_event *);
static pj_status_t on_media_finished(pjmedia_port *, void *);
static void signal_handler(int);

// header of app-control-methods
static void hangup_call();


// helper for parsing command-line-argument
static int try_get_argument(int arg, char *arg_id, char **arg_val, int argc, char *argv[])
{
	int found = 0;
	
	// check if actual argument is searched argument
	if (!strcasecmp(argv[arg], arg_id)) 
	{
		// check if actual argument has a value
		if (argc >= (arg+1))
		{
			// set value
			*arg_val = argv[arg+1];
			found = 1;
		}
	}
	return found;
}

// helper for logging messages to console (disabled if silent mode is active)
static void log_message(char *message)
{
	if (!app_cfg.silent_mode)
	{
		fprintf(stderr, "%s", message);
	}
}

// helper for setting up sip library pjsua
static void setup_sip(void)
{
	pj_status_t status;
	
	log_message("Setting up pjsua...\n");
	
	// create pjsua  
	status = pjsua_create();
	if (status != PJ_SUCCESS)
	{
		printf("Error in pjsua_create()\n");
		error_code = 2;
	}
	
	// configure pjsua	
	pjsua_config cfg;
	pjsua_config_default(&cfg);
	
	// enable just 1 simultaneous call 
	cfg.max_calls = 1;
		
	// callback configuration		
	cfg.cb.on_call_media_state = &on_call_media_state;
	cfg.cb.on_call_state = &on_call_state;
		
	// logging configuration
	pjsua_logging_config log_cfg;		
	pjsua_logging_config_default(&log_cfg);
	log_cfg.console_level = PJSUA_LOG_LEVEL;
		
	// initialize pjsua 
	status = pjsua_init(&cfg, &log_cfg, NULL);
	if (status != PJ_SUCCESS)
	{
		printf("Error in pjsua_init()\n");
		error_code = 2;
	}
	
	// add udp transport
	pjsua_transport_config udpcfg;
	pjsua_transport_config_default(&udpcfg);
		
	udpcfg.port = app_cfg.sip_port;
	status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &udpcfg, NULL);
	if (status != PJ_SUCCESS)
	{
		printf("Error creating transport\n");
		error_code = 2;
	}
	
	// initialization is done, start pjsua
	status = pjsua_start();
	if (status != PJ_SUCCESS)
	{
		printf("Error starting pjsua\n");
		error_code = 2;
	}
	
	// disable sound - use null sound device
	status = pjsua_set_null_snd_dev();
	if (status != PJ_SUCCESS)
	{
		printf("Error disabling audio\n");
		error_code = 2;
	}
}

// helper for creating and registering sip-account
static void register_sip(void)
{
	pj_status_t status;
	
	log_message("Registering account...\n");
	
	// prepare account configuration
	pjsua_acc_config cfg;
	pjsua_acc_config_default(&cfg);
	
	// build sip-user-url
	char sip_user_url[40];
	sprintf(sip_user_url, "sip:%s@%s", app_cfg.sip_user, app_cfg.sip_domain);
	
	// build sip-provder-url
	char sip_provider_url[40];
	sprintf(sip_provider_url, "sip:%s", app_cfg.sip_domain);
	
	// create and define account
	cfg.id = pj_str(sip_user_url);
	cfg.reg_uri = pj_str(sip_provider_url);
	cfg.cred_count = 1;
	cfg.cred_info[0].realm = pj_str("*");
	cfg.cred_info[0].scheme = pj_str("digest");
	cfg.cred_info[0].username = pj_str(app_cfg.sip_user);
	cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
	cfg.cred_info[0].data = pj_str(app_cfg.sip_password);
	
	// add account
	status = pjsua_acc_add(&cfg, PJ_TRUE, &acc_id);
	if (status != PJ_SUCCESS)
	{
		printf("Error adding account\n");
		error_code = 2;
	}
}

// helper for making calls over sip-account
static void make_sip_call()
{
	pj_status_t status;
	
	log_message("Starting call...\n");

	// build target sip-url
	char sip_target_url[40];
	sprintf(sip_target_url, "sip:%s@%s", app_cfg.phone_number, app_cfg.sip_domain);
	
	// start call with sip-url
	pj_str_t uri = pj_str(sip_target_url);
	status = pjsua_call_make_call(acc_id, &uri, 0, NULL, NULL, NULL);
	if (status != PJ_SUCCESS)
	{
		printf("Error making call\n");
		error_code = 2;
	}
}

// helper for creating call-media-player
static void create_player(pjsua_call_id call_id)
{
	// get call infos
	pjsua_call_info call_info; 
	pjsua_call_get_info(call_id, &call_info);
	
	pj_str_t name;
	pj_status_t status = PJ_ENOTFOUND;
	
	log_message("Creating player...\n");
	
	// create player for playback media		
	status = pjsua_player_create(pj_cstr(&name, app_cfg.audio_file), 0, &play_id);
	if (status != PJ_SUCCESS)
	{
		printf("Error playing sound-playback\n");
		error_code = 2;
	}
		
	// connect active call to media player
	pjsua_conf_connect(pjsua_player_get_conf_port(play_id), call_info.conf_slot);
	
	// get media port (play_port) from play_id
    status = pjsua_player_get_port(play_id, &play_port);
	if (status != PJ_SUCCESS)
	{
		printf("Error getting sound player port\n");
		error_code = 2;
	}
	
	// register media finished callback	
    status = pjmedia_wav_player_set_eof_cb(play_port, NULL, &on_media_finished);
	if (status != PJ_SUCCESS)
	{
		printf("Error adding sound-playback callback\n");
		error_code = 2;
	}
}

// handler for call-media-state-change-events
static void on_call_media_state(pjsua_call_id call_id)
{
	// get call infos
	pjsua_call_info call_info; 
	pjsua_call_get_info(call_id, &call_info);
	
	pj_status_t status = PJ_ENOTFOUND;

	// check state if call is established/active
	if (call_info.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
	
		log_message("Call media activated.\n");
		
		// create and start media player
		create_player(call_id);
	}
}

// handler for call-state-change-events
static void on_call_state(pjsua_call_id call_id, pjsip_event *e)
{
	// get call infos
	pjsua_call_info call_info;
	pjsua_call_get_info(call_id, &call_info);
	
	// check call state
	if (call_info.state == PJSIP_INV_STATE_CONFIRMED) 
	{
		printf("Call confirmed.\n");

		error_code = 0;
		call_confirmed = 1;

		// ensure that message is played from start
		if (play_id != PJSUA_INVALID_ID)
		{
			pjmedia_wav_player_port_set_pos(play_port, 0);
		}
	}
	if (call_info.state == PJSIP_INV_STATE_DISCONNECTED) 
	{
		hangup_call();
	}
}

// handler for media-finished-events
static pj_status_t on_media_finished(pjmedia_port *media_port, void *user_data)
{
	PJ_UNUSED_ARG(media_port);
	PJ_UNUSED_ARG(user_data);
	
	if (call_confirmed)
	{
		// count repetition
		media_counter++;
		
		// exit app if repetition limit is reached
		printf("Media Finished.\n");
		if (app_cfg.repetition_limit <= media_counter)
		{
			hangup_call();
		}
	}
	
	pj_status_t status;
	return status;
}

// display error and exit application
static void hangup_call()
{
	printf("Call disconnected, hanging up...\n");
    pjsua_call_hangup_all();
}


// helper for displaying usage infos
static void usage(int error)
{
	if (error == 1)
	{
		printf("Error, to few arguments.\n");
	}

	// print infos
	log_message("SIP Call - Automated Calls\n");
	log_message("===========================================\n");

    printf("Usage:\n");
    printf("  sipcall [options]\n");
    printf("\n");
    printf("Options:\n");
    printf("  -sd=string    Set sip provider domain       %s\n", app_cfg.sip_domain);
	printf("  -su=string    Set sip username              %s\n", app_cfg.sip_user);
	printf("  -sp=string    Set sip password              %s\n", app_cfg.sip_password);
	printf("  -so=int       Set sip port                  %d\n", app_cfg.sip_port);
	printf("  -p=string     Set target phone to call      %s\n", app_cfg.phone_number);
	printf("  -a=string     Audio file name               %s\n", app_cfg.audio_file);
	printf("  -r=int        Repeat message x-times        %d\n", app_cfg.repetition_limit);
	printf("  -t=int        Timeout to hangup call        %d\n", app_cfg.timeout);
	printf("  -s=int        Hide info messages (0/1)\n");
	printf("\n");
}

// main application
int main(int argc, char *argv[])
{
	// first set some default values
	app_cfg.audio_file = "play.wav";
	app_cfg.repetition_limit = 3;
	app_cfg.silent_mode = 0;
	app_cfg.sip_port = 5060;
	app_cfg.timeout = 50;
	app_cfg.sip_domain = "";
	app_cfg.sip_user = "";
	app_cfg.sip_password = "";
	app_cfg.phone_number = "";

	// parse arguments
	if (argc > 1)
	{
		int arg;
		for( arg = 1; arg < argc; arg+=2 )
		{
			// check if usage info needs to be displayed
			if (!strcasecmp(argv[arg], "--help"))
			{
				// display usage info and exit app
				usage(0);
				return 0;
			}

			// check if usage info needs to be displayed
			if (!strcasecmp(argv[arg], "-h"))
			{
				// display usage info and exit app
				usage(0);
				return 0;
			}

			// check for sip domain
			if (try_get_argument(arg, "-sd", &app_cfg.sip_domain, argc, argv) == 1)
			{
				continue;
			}

			// check for sip user
			if (try_get_argument(arg, "-su", &app_cfg.sip_user, argc, argv) == 1)
			{
				continue;
			}

			// check for sip password
			if (try_get_argument(arg, "-sp", &app_cfg.sip_password, argc, argv) == 1)
			{
				continue;
			}

			// check for sip port
			char *sip_port;
			if (try_get_argument(arg, "-so", &sip_port, argc, argv) == 1)
			{
				app_cfg.sip_port = atoi(sip_port); 
				continue;
			}

			// check for target phone number
			if (try_get_argument(arg, "-p", &app_cfg.phone_number, argc, argv) == 1)
			{
				continue;
			}

			// check for audio call file
			if (try_get_argument(arg, "-a", &app_cfg.audio_file, argc, argv) == 1)
			{
				continue;
			}

			// check for message repetition option
			char *repeat;
			if (try_get_argument(arg, "-r", &repeat, argc, argv) == 1)
			{
				app_cfg.repetition_limit = atoi(repeat); 
				continue;
			}

			// check for silent mode option
			char *s;
			try_get_argument(arg, "-s", &s, argc, argv);
			if (!strcasecmp(s, "1"))
			{
				app_cfg.silent_mode = 1;
				continue;
			}

			// setup the timeout to hangup a call
			char *timeout;
			if (try_get_argument(arg, "-t", &timeout, argc, argv) == 1)
			{
				app_cfg.timeout = atoi(timeout);
				continue;
			}

			// Wrong argument added
			usage(1);
			return -1;
		}
	} 
	else
	{
		// no arguments specified - display usage info and exit app
		usage(1);
		return error_code;
	}
	
	if (!app_cfg.sip_domain || !app_cfg.sip_user || !app_cfg.sip_password || !app_cfg.phone_number)
	{
		// Too few arguments specified - display usage info and exit app
		usage(1);
		return error_code;
	}
    if (access(app_cfg.audio_file, F_OK) != 0) {
    	// Checks if the audio file exists
        printf("Audio file doesn't exist: %s\n", app_cfg.audio_file);
		return error_code;
    }

	
	// register signal handler for break-in-keys (e.g. ctrl+c)
	signal(SIGINT, hangup_call);
	signal(SIGKILL, hangup_call);
	
	// setup up sip library pjsua
	setup_sip();
	
	// create account and register to sip server
	register_sip();
	
	// initiate call
	make_sip_call();
	

    // Wait for the call to disconnect or timeout after timeout seconds
    pj_timestamp start_time, current_time;
    pj_time_val elapsed_ms;
    pj_get_timestamp(&start_time);

    while (pjsua_call_get_count() > 0) {
        pj_get_timestamp(&current_time);
        pj_time_val elapsed_time = pj_elapsed_time(&start_time, &current_time);

        if (elapsed_time.sec >= app_cfg.timeout) {
            printf("Timeout reached, hanging up all calls...\n");
            hangup_call();
            break;
        }

        pj_thread_sleep(1000); // Sleep for 1 second
    }

    // Destroy PJSUA
    pjsua_destroy();
	
	return error_code;
}
