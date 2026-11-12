// AURA Bass — registro de autoria
// Copyright (c) 2026 Thiago Galvão. Todos os direitos reservados.
// Obra protegida pelas Leis 9.609/1998 e 9.610/1998. Uso pessoal permitido;
// cópia, redistribuição ou obra derivada apenas com autorização por escrito.
// Licença: ver LICENSE  |  https://github.com/thiagorgalvao-sketch/AURA
//
// Este arquivo é a única fonte dos dados de autoria. As cadeias abaixo ficam
// gravadas dentro do binário do plugin e aparecem no menu "Sobre". Remover ou
// alterar esses avisos é vedado pela licença; a marca AURA-PROV abaixo também
// serve para identificar cópias do binário (basta rodar `strings` no arquivo).

#pragma once

namespace AuraAutoria
{
    inline constexpr const char* obra       = "AURA Bass — overdrive e pré de baixo";
    inline constexpr const char* autor      = "Thiago Galvão";
    inline constexpr const char* copyright  = "Copyright (c) 2026 Thiago Galvão";
    inline constexpr const char* direitos   = "Todos os direitos reservados — Leis 9.609/1998 e 9.610/1998";
    inline constexpr const char* site       = "github.com/thiagorgalvao-sketch/AURA";
    inline constexpr const char* criacao    = "Criação iniciada em 23/10/2026";

    // Marca de origem embutida no binário, para identificar cópias.
    inline constexpr const char* marca =
        "AURA-PROV/1 obra=AURA-Bass autor=Thiago-Galvao ano=2026 "
        "lei=9609/1998+9610/1998 origem=github.com/thiagorgalvao-sketch/AURA "
        "direitos=reservados";

    // Mantém as cadeias no binário mesmo sem uso direto.
    struct Selo { const char* const* v; };
    inline const char* const tabela[] = { obra, autor, copyright, direitos, site, criacao, marca, nullptr };
    inline const Selo selo { tabela };
}
