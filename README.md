# php-spy
WIP! Early stages! Do not try this at home :)

```
> phpize
> ./configure --enable-spy
> make && make install
> echo "zend_extension=$PATH_TO_EXTENSIONS/spy.so" >> $PHP_ROOT/conf/php.d/spy.ini
> php -m | grep Spy # verify it's loaded
```
