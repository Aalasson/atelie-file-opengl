#version 330 core
in vec3 vPosition;
in vec3 vNormal;
in vec2 vUV;
out vec4 outColor;
uniform sampler2D uTexture;
uniform vec3 uColor;
uniform float uSpecular;
uniform float uShininess;
uniform float uUVScale;
uniform float uEmission;
uniform vec3 uEye;
uniform vec3 uLampPosition;
uniform int uLampOn;
vec3 blinnPhong(vec3 normal,vec3 viewDirection,vec3 lightDirection,vec3 radiance,vec3 base){
    float diffuse=max(dot(normal,lightDirection),0.0);
    vec3 halfDirection=normalize(lightDirection+viewDirection);
    float specular=diffuse>0.0?pow(max(dot(normal,halfDirection),0.0),max(uShininess,1.0))*uSpecular:0.0;
    return (base*diffuse+vec3(specular))*radiance;
}
void main(){
    vec3 base=texture(uTexture,vUV*uUVScale).rgb*uColor;
    vec3 normal=normalize(vNormal);
    if(!gl_FrontFacing)normal=-normal;
    vec3 viewDirection=normalize(uEye-vPosition);
    float hemisphere=0.5+0.5*normal.y;
    vec3 ambient=mix(vec3(0.18,0.16,0.14),vec3(0.39,0.42,0.45),hemisphere);
    vec3 color=ambient*base;
    color+=blinnPhong(normal,viewDirection,normalize(vec3(-0.55,0.90,0.45)),vec3(0.94,0.89,0.79)*0.90,base);
    if(uLampOn!=0){
        vec3 toLight=uLampPosition-vPosition;
        float distanceToLight=length(toLight);
        float attenuation=1.0/(1.0+0.22*distanceToLight+0.16*distanceToLight*distanceToLight);
        color+=blinnPhong(normal,viewDirection,normalize(toLight),vec3(1.0,0.71,0.38)*attenuation*2.1,base);
    }
    color+=base*uEmission;
    color=vec3(1.0)-exp(-color*1.10);
    color=pow(max(color,vec3(0.0)),vec3(1.0/2.2));
    float fog=smoothstep(18.0,48.0,length(uEye-vPosition))*0.25;
    outColor=vec4(mix(color,vec3(0.84,0.86,0.86),fog),1.0);
}
