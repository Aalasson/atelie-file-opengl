#version 330 core
in vec2 vUV;
in vec4 vColor;
out vec4 outColor;
uniform sampler2D uAtlas;
void main(){
    float coverage=texture(uAtlas,vUV).r;
    outColor=vec4(vColor.rgb,vColor.a*coverage);
}
