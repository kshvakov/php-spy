PHP_ARG_ENABLE(spy, whether to enable spy extension support, 
  [--enable-spy Enable spy extension support])

if test $PHP_SPY != "no"; then
    PHP_NEW_EXTENSION(spy, php_spy.c, $ext_shared)
fi
