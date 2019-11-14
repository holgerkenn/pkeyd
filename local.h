#
#define CONFIGFILE	"./dpkeyd.conf"

#define KEYDB		"./keydb"
#define SECUREDIR	"."
#define LOGFILE		"./dpkeyd.log"
#define LOCKFILE	"./dpkeyd.lock"
#define MYDOMAIN	"my.domain"
#define MYADRESS	"host.my.domain"
#define ROUTEFILE	"./dpkeyd.route"
#define SERVICE		"5000" /* EXPERIMENTAL */

/* uncomment this to get into daemon mode which forks and logs to logfile */
/* #define SERVER_MODE */

/* uncomment this to query nameservice for incomming connections */
/* #define NS_QUERY */

/* uncomment this to see a lot of debug messages in the logfile */
/* #define DEBUG */ 
