ipkg: 
	tar -C vtuner-client -xf vtunerc-root.tar
	cp ../dist/mipsel/vtunerc.mipsel vtuner-client/usr/sbin/
	./ipkg-build vtuner-client
	
clean:
	rm -rf vtuner-client/usr vtuner-client/etc
	rm -f vtuner-client_*_mipsel.ipk
