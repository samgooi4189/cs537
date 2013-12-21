import subprocess, time, os
from test import Test, Failure
import httplib, random, threading

class ServerTest(Test):
   timeout = 30

   def client_run(self, conn):
      try:
         response = conn.getresponse()
         msg = response.read()
         conn.close()
         if (len(msg) == 0):
            self.fail("missing body in response")
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))
         raise Failure("Client failed with error: " + str(inst))

   def run_server(self, threads, buffers, schedalg, n=None):
      minport = 5000
      maxport = 10000
      root = os.path.join(self.test_path, "files")
      for i in range(5):
         port = random.randint(minport, maxport)
         self.log("Starting server on port " + str(port))
         args = [str(port), str(threads), str(buffers), schedalg]
         if n is not None:
            args.append(str(n))
         serverProc = self.startexe("server", args) #, cwd=root)
         time.sleep(0.2)
         # wait for sever to respond
         serverProc.poll()
         if serverProc.returncode is None:
#            conn = httplib.HTTPConnection("localhost", port, timeout=2)
#            conn.request("GET", "/home.html")
#            response = conn.getresponse()
#            response.read()
            serverProc.poll()
            if serverProc.returncode is None:
               time.sleep(0.1)
               self.port = port
               self.serverProc = serverProc
               return serverProc
         try:
            serverProc.kill()
         except:
            pass
      raise Failure("Could not start server")

class UsageStatisticsTest(ServerTest):
   stat_name = None
   def get_header(self, hdr, name):
      hdr_list = hdr.__str__().split("\n")
      value = None
      for h in hdr_list:
         h_list = h.split(":")
         if h_list[0].strip() == name:
            value = h_list[1].strip()
            return value
      return value

class Arrival(UsageStatisticsTest):
   name = "arrival"
   stat_name = "Stat-req-arrival"
   description = "check " + stat_name
   def client1_run(self):
      try:
         conn = httplib.HTTPConnection("localhost", self.port, timeout=2)
         send_time = int(time.time()*1000) # in seconds
         conn.request("GET", "/home.html")
         response = conn.getresponse()

         msg = response.read()
         if (len(msg) == 0):
            self.fail("missing body in response")

         arrival_time = self.get_header(response.msg, self.stat_name)
         if arrival_time is None:
            self.fail(self.stat_name + " not in header")
         else:
            arrival_time = int(arrival_time)
            if arrival_time - send_time > 2000:
               self.fail("Arrival time differs from send time by more than 2 seconds")
            self.log(("send: %d, arrival: %d") % (send_time, arrival_time))
         conn.close()
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))

   threads = 1
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()

class Dispatch(UsageStatisticsTest):
   name = "dispatch"
   stat_name = "Stat-req-dispatch"
   description = "check " + stat_name
   def client1_run(self):
      for t in [1, 3]:
         try:
            conns = list()
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=t+1))
            conns[-1].request("GET", "/output.cgi?" + str(t))
            time.sleep(0.1)
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=t+1))
            conns[-1].request("GET", "/home.html")

            r1 = conns[0].getresponse()
            r2 = conns[1].getresponse()

            if (len(r1.read()) == 0 or len(r2.read()) == 0):
               self.fail("missing body in response")

            dispatch = self.get_header(r2.msg, self.stat_name)
            if dispatch is None:
               self.fail(self.stat_name + " not in header")
            else:
               dispatch = int(dispatch)
               min_val = (t * 1000) - 500 # 500ms to allow some delay
               max_val = (t + 1) * 1000
               if not (min_val <= dispatch and dispatch <= max_val):
                  self.fail("Dispatch interval not in expected range (%d - %d)" % (min_val, max_val))
               self.log(("dispatch: %d") % (dispatch))

            for c in conns:
               c.close()
         except Exception as inst:
            self.fail("Client failed with error: " + str(inst))
         if self.is_failed():
            break

   threads = 1
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()


class ReadTest(UsageStatisticsTest):
   name = "read"
   stat_name = "Stat-req-read"
   description = "check " + stat_name
   def client1_run(self):
      try:
         conn = httplib.HTTPConnection("localhost", self.port, timeout=2)
         conn.request("GET", "/numbers.html")
         response = conn.getresponse()

         msg = response.read()
         if (len(msg) == 0):
            self.fail("missing body in response")

         t1 = self.get_header(response.msg, self.stat_name)
         if t1 is None:
            self.fail(self.stat_name + " not in header")
         else:
            t1 = int(t1)
            min_val = 1
            max_val = 10
            if not (min_val <= t1 and t1 <= max_val):
                  self.fail("read interval not in expected range (%d - %d)" % (min_val, max_val))
            self.log(("read: %d") % (t1))
         conn.close()
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))

   threads = 1
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()


class Complete(UsageStatisticsTest):
   name = "complete"
   stat_name = "Stat-req-complete"
   description = "check " + stat_name
   def client1_run(self):
      for t in [1, 3]:
         try:
            conns = list()
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=t+1))
            conns[-1].request("GET", "/output.cgi?" + str(t))
            time.sleep(0.1)
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=t+1))
            conns[-1].request("GET", "/home.html")

            r1 = conns[0].getresponse()
            r2 = conns[1].getresponse()

            if (len(r1.read()) == 0 or len(r2.read()) == 0):
               self.fail("missing body in response")

            stat_val = self.get_header(r2.msg, self.stat_name)
            if stat_val is None:
               self.fail(self.stat_name + " not in header")
            else:
               stat_val = int(stat_val)
               min_val = (t * 1000) - 500 # 500ms to allow some delay
               max_val = (t + 1) * 1000
               if not (min_val <= stat_val and stat_val <= max_val):
                  self.fail("Complete interval not in expected range (%d - %d)" % (min_val, max_val))
               self.log(("complete: %d") % (stat_val))

            for c in conns:
               c.close()
         except Exception as inst:
            self.fail("Client failed with error: " + str(inst))
         if self.is_failed():
            break

   threads = 1
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()


class ThreadId(UsageStatisticsTest):
   name = "threadid"
   stat_name = "Stat-thread-id"
   description = "check " + stat_name
   def client1_run(self):
      num_reqs = 4
      num_threads = 3
      try:
         conns = list()
         for i in range(num_reqs):
            time.sleep(0.1)
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=3))
            conns[-1].request("GET", "/output.cgi?1")

         responses = list()
         for i in range(num_reqs):
            responses.append(conns[i].getresponse())
            msg = responses[-1].read()
            if (len(msg) == 0):
               self.fail("missing body in response")
         for c in conns:
            c.close()

         ids = range(num_threads)
         for i in range(num_reqs):
            stat_val = self.get_header(responses[i].msg, self.stat_name)
            if stat_val is None:
               self.fail(self.stat_name + " not in header")
               return
            else:
               stat_val = int(stat_val)
               if stat_val >= num_threads:
                  self.fail("thread-id greater than (total threads - 1)")
                  return
               else:
                  ids[stat_val] = -1
         
         missing_ids = list()
         for i in ids:
            if i != -1:
               missing_ids.append(i)

         if len(missing_ids) != 0:
            self.fail("missing response from thread-ids: " + str(missing_ids))
            
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))

   threads = 3
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()

class ThreadCount(UsageStatisticsTest):
   name = "threadcount"
   stat_name = "Stat-thread-count"
   description = "check Stat-thread-count, Stat-thread-static, and Stat-thread-dynamic"
   def client1_run(self):
      num_threads = 2
      requests = ["/output.cgi?2", "/home.html", "/home.html"]
      num_reqs = len(requests)
      try:
         conns = list()
         for request in requests:
            time.sleep(0.1)
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=5))
            conns[-1].request("GET", request)

         responses = list()
         for i in range(num_reqs):
            responses.append(conns[i].getresponse())
            msg = responses[-1].read()
            if (len(msg) == 0):
               self.fail("missing body in response")
         for c in conns:
            c.close()

         ids = range(num_threads)
         expected_values = [(1, 0, 1), (1, 1, 0), (2, 2, 0)]
         seen_values = list()
         for i in range(num_reqs):
            thread_count = self.get_header(responses[i].msg, "Stat-thread-count")
            thread_static = self.get_header(responses[i].msg, "Stat-thread-static")
            thread_dynamic = self.get_header(responses[i].msg, "Stat-thread-dynamic")
            
            if thread_count is None:
               self.fail("Stat-thread-count not in header")
               return
            else:
               thread_count = int(thread_count)
            if thread_static is None:
               self.fail("Stat-thread-static not in header")
               return
            else:
               thread_static = int(thread_static)
            if thread_dynamic is None:
               self.fail("Stat-thread-dynamic not in header")
               return
            else:
               thread_dynamic = int(thread_dynamic)
               
            val = (thread_count, thread_static, thread_dynamic)
            if val in expected_values:
               expected_values.remove(val)
               seen_values.append(val)
            elif val in seen_values:
               self.fail("repeated Stat-thread counts")
            else:
               self.fail("Incorrect Stat-thread counts")
            self.log("Stat-thread-count: %d, Stat-thread-static: %d, Stat-thread-dynamic: %d" %
                     (thread_count, thread_static, thread_dynamic))
            
            if self.is_failed():
               break
                 
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))

   threads = 2
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()


class ThreadCount2(UsageStatisticsTest):
   name = "threadcount2"
   stat_name = "Stat-thread-count"
   description = "check Stat-thread-count, Stat-thread-static, and Stat-thread-dynamic"
   def client1_run(self):
      num_threads = 2
      requests = ["/output.cgi?1", "/output.cgi?1"]
      num_reqs = len(requests)
      try:
         conns = list()
         for request in requests:
            time.sleep(0.1)
            conns.append(httplib.HTTPConnection("localhost", self.port, timeout=5))
            conns[-1].request("GET", request)

         responses = list()
         for i in range(num_reqs):
            responses.append(conns[i].getresponse())
            msg = responses[-1].read()
            if (len(msg) == 0):
               self.fail("missing body in response")
         for c in conns:
            c.close()

         ids = range(num_threads)
         for i in range(num_reqs):
            thread_count = self.get_header(responses[i].msg, "Stat-thread-count")
            thread_static = self.get_header(responses[i].msg, "Stat-thread-static")
            thread_dynamic = self.get_header(responses[i].msg, "Stat-thread-dynamic")
            
            if thread_count is None:
               self.fail("Stat-thread-count not in header")
               return
            else:
               thread_count = int(thread_count)
            if thread_static is None:
               self.fail("Stat-thread-static not in header")
               return
            else:
               thread_static = int(thread_static)
            if thread_dynamic is None:
               self.fail("Stat-thread-dynamic not in header")
               return
            else:
               thread_dynamic = int(thread_dynamic)
               
            val = (thread_count, thread_static, thread_dynamic)
            if val != (1, 0, 1):
               self.fail("Incorrect Stat-thread counts")
            self.log("Stat-thread-count: %d, Stat-thread-static: %d, Stat-thread-dynamic: %d" %
                     (thread_count, thread_static, thread_dynamic))
            
            if self.is_failed():
               break
                 
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))

   threads = 2
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()

class Basic(ServerTest):
   name = "basic"
   description = "check if the webserver can serve requests"

   def client1_run(self):
      try:
         conn = httplib.HTTPConnection("localhost", self.port, timeout=2)
         conn.request("GET", "/home.html")
         response = conn.getresponse()

         msg = response.read()

         if (len(msg) == 0):
            self.fail("missing body in response")

         conn.close()
      except Exception as inst:
         self.fail("Client failed with error: " + str(inst))

   threads = 1
   buffers = 1
   schedalg = "FIFO"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(self.threads, self.buffers, self.schedalg)
      else:
         self.port = port
      self.client1_run()
      self.done()


class Fifo(ServerTest):
   name = "fifo"
   description = "FIFO with dynamic requests and one thread"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=1, buffers=1, schedalg="FIFO")
      else:
         self.port = port
      requests = ["/output.cgi?3", "/3.cgi?1", "/2.cgi?1", "/1.cgi?1"]
      conns = list()
      for request in requests:
         time.sleep(0.1)
         connection = httplib.HTTPConnection("localhost", self.port, timeout=10)
         connection.request("GET", request)
         conns.append(connection)

      clients = [threading.Thread(target=self.client_run, args=(conn,))
            for conn in conns]
      for client in clients:
         client.start()

      for i in range(4):
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
         for j in range(i+1, 4):
            if not clients[j].is_alive():
               self.fail("reply " + str(j) + " was received before reply " + str(i))
      self.done()

class Sff(ServerTest):
   name = "sff"
   description = "Smallest file first with dynamic requests and one thread"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=1, buffers=10, schedalg="SFF")
      else:
         self.port = port
      conns = [None] * 4

      conns[0] = httplib.HTTPConnection("localhost", self.port, timeout=4)
      conns[0].request("GET", "/output.cgi?3")

      time.sleep(1)

      conns[2] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[2].request("GET", "/2.cgi?1")

      time.sleep(0.1)

      conns[3] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[3].request("GET", "/3.cgi?1")

      time.sleep(0.1)

      conns[1] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[1].request("GET", "/1.cgi?1")

      clients = [threading.Thread(target=self.client_run, args=(conn,)) for conn in conns]

      for client in clients:
         client.start()

      for i in range(4):
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
         for j in range(i+1, 4):
            if not clients[j].is_alive():
               self.fail("reply " + str(j) + " was received before reply " + str(i))
      self.done()


class Sff2(ServerTest):
   name = "sff2"
   description = "Smallest file first with dynamic requests"
   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=2, buffers=10, schedalg="SFF")
      else:
         self.port = port
      conns = [None] * 6
      conns[0] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[0].request("GET", "/output.cgi?2")
      conns[1] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[1].request("GET", "/output.cgi?3")
      time.sleep(1)
      conns[3] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[3].request("GET", "/2.cgi?1")
      conns[4] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[4].request("GET", "/2.cgi?1")
      conns[5] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[5].request("GET", "/3.cgi?1")
      conns[2] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[2].request("GET", "/1.cgi?1")
      clients = [threading.Thread(target=self.client_run, args=(conn,)) for conn in conns]
      for client in clients:
         client.start()
      for i in [2, 5]:
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
      for client in clients:
         client.join()
      self.done()

class SffBs(ServerTest):
   name = "sffbs"
   description = "SFF-BS with num epochs > num requests"
   
   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=2, buffers=8, schedalg="SFF-BS", n=8)
      else:
         self.port = port
      conns = [None] * 6
      conns[0] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[0].request("GET", "/output.cgi?2")
      conns[1] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[1].request("GET", "/output.cgi?3")
      time.sleep(1)
      conns[3] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[3].request("GET", "/2.cgi?1")
      conns[4] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[4].request("GET", "/2.cgi?1")
      conns[5] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[5].request("GET", "/3.cgi?1")
      conns[2] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[2].request("GET", "/1.cgi?1")
      clients = [threading.Thread(target=self.client_run, args=(conn,)) for conn in conns]
      for client in clients:
         client.start()
      for i in [2, 5]:
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
      for client in clients:
         client.join()
      self.done()


class SffBs2(ServerTest):
   name = "sffbs2"
   description = "SFF-BS with smaller file in second epoch"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=1, buffers=10, schedalg="SFF-BS", n=3)
      else:
         self.port = port
      conns = [None] * 4

      conns[0] = httplib.HTTPConnection("localhost", self.port, timeout=4)
      conns[0].request("GET", "/output.cgi?3")

      time.sleep(1)

      conns[2] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[2].request("GET", "/output.cgi?1")

      time.sleep(0.1)

      conns[1] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[1].request("GET", "/2.cgi?1")

      time.sleep(0.1)

      conns[3] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[3].request("GET", "/1.cgi?1")

      clients = [threading.Thread(target=self.client_run, args=(conn,)) for conn in conns]

      for client in clients:
         client.start()

      for i in range(4):
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
         for j in range(i+1, 4):
            if not clients[j].is_alive():
               self.fail("reply " + str(j) + " was received before reply " + str(i))
      self.done()


class SffBs3(ServerTest):
   name = "sffbs3"
   description = "SFF-BS with three epochs of size 2"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=1, buffers=10, schedalg="SFF-BS", n=2)
      else:
         self.port = port
      conns = [None] * 5

      conns[0] = httplib.HTTPConnection("localhost", self.port, timeout=4)
      conns[0].request("GET", "/output.cgi?3")

      time.sleep(1)

      conns[1] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[1].request("GET", "/3.cgi?1")

      time.sleep(0.1)

      conns[3] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[3].request("GET", "/2.cgi?1")

      time.sleep(0.1)

      conns[2] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[2].request("GET", "/1.cgi?1")

      time.sleep(0.1)

      conns[4] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[4].request("GET", "/1.cgi?1")

      clients = [threading.Thread(target=self.client_run, args=(conn,)) for conn in conns]

      for client in clients:
         client.start()

      for i in range(5):
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
         for j in range(i+1, 4):
            if not clients[j].is_alive():
               self.fail("reply " + str(j) + " was received before reply " + str(i))
      self.done()

   
class SffBs4(ServerTest):
   name = "sffbs4"
   description = "SFF-BS with epoch size 1"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=1, buffers=1, schedalg="SFF-BS", n=1)
      else:
         self.port = port
      requests = ["/output.cgi?3", "/3.cgi?1", "/2.cgi?1", "/1.cgi?1"]
      conns = list()
      for request in requests:
         time.sleep(0.1)
         connection = httplib.HTTPConnection("localhost", self.port, timeout=10)
         connection.request("GET", request)
         conns.append(connection)

      clients = [threading.Thread(target=self.client_run, args=(conn,))
            for conn in conns]
      for client in clients:
         client.start()

      for i in range(4):
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
         for j in range(i+1, 4):
            if not clients[j].is_alive():
               self.fail("reply " + str(j) + " was received before reply " + str(i))
      self.done()


class SffBs5(ServerTest):
   name = "sffbs5"
   description = "SFF-BS with epoch size 4"

   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=3, buffers=5, schedalg="SFF-BS", n=4)
      else:
         self.port = port
      conns = [None] * 6

      conns[0] = httplib.HTTPConnection("localhost", self.port, timeout=4)
      conns[0].request("GET", "/output.cgi?3")

      time.sleep(0.1)

      conns[1] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[1].request("GET", "/output.cgi?2")

      time.sleep(0.1)

      conns[2] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[2].request("GET", "/output.cgi?3")

      time.sleep(0.1)

      conns[3] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[3].request("GET", "/3.cgi?1")

      time.sleep(0.1)

      conns[5] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[5].request("GET", "/2.cgi?1")

      time.sleep(0.1)

      conns[4] = httplib.HTTPConnection("localhost", self.port, timeout=10)
      conns[4].request("GET", "/1.cgi?1")

      clients = [threading.Thread(target=self.client_run, args=(conn,)) for conn in conns]

      for client in clients:
         client.start()

      for i in [3, 4]:
         if not clients[i].is_alive():
            self.fail("reply " + str(i) + " received too soon")
            continue
         clients[i].join()
         for j in range(i+1, 4):
            if not clients[j].is_alive():
               self.fail("reply " + str(j) + " was received before reply " + str(i))
      self.done()


class Pool(ServerTest):
   name = "pool"
   description = "check if using a fixed size thread pool"
   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=2, buffers=8, schedalg="FIFO")
      else:
         self.port = port
      conns = list()

      for i in range(4):
         conns.append(httplib.HTTPConnection("localhost", self.port, timeout=10))
         conns[-1].request("GET", "/output.cgi?1")
         time.sleep(0.1)

      conns[0].getresponse()
      conns[1].getresponse()
      t1 = time.time()

      conns[2].getresponse()
      conns[3].getresponse()
      t2 = time.time()

      if t2 < t1 + 1:
         self.fail("not using a fixed size thread pool (size 2)")
      self.done()


class Pool2(ServerTest):
   name = "pool2"
   description = "check if using a fixed size thread pool"
   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=3, buffers=8, schedalg="FIFO")
      else:
         self.port = port
      conns = list()

      for i in range(6):
         conns.append(httplib.HTTPConnection("localhost", self.port, timeout=10))
         conns[-1].request("GET", "/output.cgi?1")
         time.sleep(0.1)

      conns[0].getresponse()
      conns[1].getresponse()
      conns[2].getresponse()
      t1 = time.time()

      conns[3].getresponse()
      conns[4].getresponse()
      conns[5].getresponse()
      t2 = time.time()

      if t2 < t1 + 1:
         self.fail("not using a fixed size thread pool (size 2)")
      self.done()


class Locks(ServerTest):
   name = "locks"
   description = "many concurrent requests to test locking"
   threads = 8
   buffers = 16
   schedalg = "FIFO"
   num_clients = 20
   loops = 20
   requests = ["/home.html", "/output.cgi?0.3"]
   def many_reqs(self):
      for i in range(self.loops):
         for request in self.requests:
            conn = httplib.HTTPConnection("localhost", self.port, timeout=8)
            conn.request("GET", request)
            self.client_run(conn)
   def run(self, port=None):
      if port is None:
         serverProc = self.run_server(threads=self.threads, buffers=self.buffers, schedalg=self.schedalg)
      else:
         self.port = port
      clients = [threading.Thread(target=self.many_reqs) for i in range(self.num_clients)]
      for client in clients:
         client.start()
      for client in clients:
         client.join()
      self.done()

class Locks2(Locks):
   name = "locks2"
   threads = 32
   buffers = 64
   num_clients = 40
   loops = 10
   requests = ["/home.html", "/output.cgi?0.3", "/in", "/output.cgi?0.2"]

class Locks3(Locks):
   name = "locks3"
   threads = 64
   buffers = 128
   schedalg = "SFF"
   num_clients = 50
   loops = 6
   requests = ["/home.html", "/output.cgi?0.3", "/in", "/output.cgi?0.2"]
   timeout = 60

class Locks4(Locks):
   name = "locks4"
   threads = 25
   buffers = 27
   num_clients = 20
   loops = 20
   requests = ["/output.cgi?0.01", "/output.cgi?0.02", "/output.cgi?0.005"]
   timeout = 100

class Fewer(Locks):
   name = "fewer"
   description = "fewer buffers than threads"
   threads = 16
   buffers = 8
   num_clients = 20
   loops = 20

class Fewer2(Locks):
   name = "fewer2"
   description = "fewer buffers than threads"
   threads = 32
   buffers = 1
   num_clients = 20
   loops = 20


test_list = [Basic, Fifo, Sff, Sff2, SffBs, SffBs2, SffBs3, SffBs4, SffBs5, Pool, Pool2, Locks, Locks2, Locks3, Locks4, Fewer, Fewer2, Arrival, Dispatch, Complete, ThreadId, ThreadCount, ThreadCount2]

