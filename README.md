# php-spy
WIP! Early stages! Do not try this at home :)

```
> phpize
> ./configure --enable-spy
> make && make install
> EXTENSION_DIR=$(php -i | grep "^extension_dir" | cut -d' ' -f 5)
> PHPD_DIR=$(php -i | grep "Scan this dir for additional .ini files" | cut -d ' ' -f 9)
> echo "zend_extension=$EXTENSION_DIR/spy.so" >> $PHPD_DIR/spy.ini
> php -m | grep Spy # verify it's loaded
```
