-- example script that demonstrates response handling and
-- retrieving an authentication token to set on all future
-- requests

token = nil
path  = "/authenticate"

request = function()
   return wrk.format("GET", path)
end

response = function(status, headers, body)
   if not token and status == 200 then
      token = headers["X-Token"]
      path  = "/resource"
      wrk.headers["X-Token"] = token
   end
end
