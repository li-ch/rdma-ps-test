make clean
make test
rsync test 172.16.18.205::upload
rsync test 172.16.18.198::upload
rsync test 172.16.18.197::upload
# rsync test 100.88.65.7::upload
# rsync test 100.88.65.8::upload
# rsync test 100.88.65.9::upload
# rsync test 100.88.65.10::upload
echo "rsync done"
