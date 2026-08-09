struct p101_env;
struct p101_error;
#define P101_SEMANTIC_ROLE(role) __attribute__((annotate(role)))
int                p101_open(const struct p101_env *, struct p101_error *, const char *, int, ...);
struct p101_error *p101_error_create(_Bool) P101_SEMANTIC_ROLE("p101:ownership:error:acquire");
void               p101_error_destroy(struct p101_error *) P101_SEMANTIC_ROLE("p101:ownership:error:release");
struct p101_env   *p101_env_create(struct p101_error *, void *) P101_SEMANTIC_ROLE("p101:ownership:env:acquire");
void               p101_env_destroy(struct p101_env *) P101_SEMANTIC_ROLE("p101:ownership:env:release");
void               exit(int);
void               _exit(int);
#define P101_TRACE_SCOPE(value) ((void)(value))
#define NULL ((void *)0)

static struct p101_env   *env;
static struct p101_error *err;

static void bad(void)
{
    P101_TRACE_SCOPE(env);
    p101_open(env, err, "x", 0);
    p101_open(env, NULL, "x", 0);
    p101_open(env, err, "x", 0);
}

static void good(const struct p101_env *function_env, struct p101_error *function_err)
{
    p101_open(function_env, function_err, "x", 0);
}

static void ownership_gap(void)
{
    (void)p101_error_create(0);
    (void)p101_env_create(err, NULL);
}

static void arbitrary_termination(void) P101_SEMANTIC_ROLE("p101:test:negative-control:process-termination");

static void arbitrary_termination(void)
{
    exit(7);
}

static void p101_tool_run_child_main(void) P101_SEMANTIC_ROLE("p101:termination-adapter");

static void p101_tool_run_child_main(void)
{
    _exit(127);
}

static int multiple_returns(int value)
{
    if(value != 0)
    {
        return 1;
    }
    return 0;
}

static int source_value(void)
{
    return 1;
}

static int consume_value(int value)
{
    return value;
}

static int embedded_condition(void)
{
    int result;

    result = 0;
    if(source_value())
    {
        result = 1;
    }
    return result;
}

static int embedded_argument(void)
{
    int result;

    result = consume_value(source_value());
    return result;
}

static int embedded_return(void)
{
    return source_value();
}

int main(void)
{
    (void)multiple_returns(0);
    (void)embedded_condition();
    (void)embedded_argument();
    (void)embedded_return();
    exit(0);
}
