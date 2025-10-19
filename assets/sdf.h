float sdTriangle(float2 p, float2 p0, float2 p1, float2 p2)
{
  float2 e0 = p1-p0, e1 = p2-p1, e2 = p0-p2;
  float2 v0 = p -p0, v1 = p -p1, v2 = p -p2;
  float2 pq0 = v0 - e0*clamp( dot(v0,e0)/dot(e0,e0), 0.0, 1.0 );
  float2 pq1 = v1 - e1*clamp( dot(v1,e1)/dot(e1,e1), 0.0, 1.0 );
  float2 pq2 = v2 - e2*clamp( dot(v2,e2)/dot(e2,e2), 0.0, 1.0 );
  float s = sign( e0.x*e2.y - e0.y*e2.x );
  float2 d = min(min(float2(dot(pq0,pq0), s*(v0.x*e0.y-v0.y*e0.x)),
                     float2(dot(pq1,pq1), s*(v1.x*e1.y-v1.y*e1.x))),
                     float2(dot(pq2,pq2), s*(v2.x*e2.y-v2.y*e2.x)));
  return -sqrt(d.x)*sign(d.y);
}

float sdBox(float2 p, float2 b)
{
  float2 d = abs(p)-b;
  return length(max(d,0.0)) + min(max(d.x,d.y),0.0);
}

float sdCircle(float2 p, float r)
{
  return length(p) - r;
}
