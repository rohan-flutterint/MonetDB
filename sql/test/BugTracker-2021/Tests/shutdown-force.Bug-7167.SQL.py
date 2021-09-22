import os, socket, tempfile, sys, pymonetdb

try:
    from MonetDBtesting import process
except ImportError:
    import process

def freeport():
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.bind(('', 0))
    port = sock.getsockname()[1]
    sock.close()
    return port

myport = freeport()

with tempfile.TemporaryDirectory() as farm_dir:
    os.mkdir(os.path.join(farm_dir, 'db1'))
    with process.server(mapiport=myport, dbname='db1', dbfarm=os.path.join(farm_dir, 'db1'),
                        stdin=process.PIPE, stdout=process.PIPE, stderr=process.PIPE) as srv:
        conn = pymonetdb.connect(database='db1', port=myport, autocommit=True)
        cur = conn.cursor()
        try:
            cur.execute("call sys.shutdown(1, true);")
            sys.stderr.write('Exception expected')
        except:
            pass
        cur.close()
        conn.close()
        out, err = srv.communicate()
        sys.stderr.write(err)
